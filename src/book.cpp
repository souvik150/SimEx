#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include <vector>

#include <curses.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "snapshot/SnapshotLayout.h"
#include "types/AppTypes.h"

namespace {

constexpr int kDisplayLevels = 14;
constexpr auto kPollInterval = std::chrono::milliseconds(60);
constexpr auto kFlashDuration = std::chrono::milliseconds(450);

using Clock = std::chrono::steady_clock;
using FlashMap = std::unordered_map<std::string, Clock::time_point>;

bool gColorsEnabled = false;

constexpr short kPairBidText = 1;
constexpr short kPairAskText = 2;
constexpr short kPairHeader = 3;
constexpr short kPairFooter = 4;
constexpr short kPairBidDepth = 5;
constexpr short kPairAskDepth = 6;
constexpr short kPairHeadline = 7;
constexpr short kColorBidBg = 20;
constexpr short kColorAskBg = 21;
constexpr short kColorHeadline = 22;

struct Level {
    double price = 0.0;
    double qty = 0.0;
};

struct Snapshot {
    InstrumentToken token = 0;
    std::string instrument;
    std::string timestamp;
    double ltp = 0.0;
    double ltq = 0.0;
    std::vector<Level> bids;
    std::vector<Level> asks;
};

std::string formatTimestamp(uint64_t ns) {
    using namespace std::chrono;
    const auto tp = time_point<system_clock>(nanoseconds(ns));
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

std::string formatPrice(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

std::string formatQty(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(0) << value;
    return out.str();
}

struct SharedSnapshotReader {
    snapshot::SharedSnapshot* ptr = nullptr;
    std::size_t size = 0;
    int fd = -1;
    uint64_t last_seq = 0;
};

std::string shmRegionName(const std::string& prefix, InstrumentToken token) {
    std::string name = prefix;
    if (name.empty() || name.front() != '/') {
        name = "/" + name;
    }
    name += "_" + std::to_string(token);
    return name;
}

std::optional<SharedSnapshotReader> mapSharedSnapshot(const std::string& prefix, InstrumentToken token) {
    SharedSnapshotReader reader;
    const auto name = shmRegionName(prefix, token);
    reader.fd = ::shm_open(name.c_str(), O_RDONLY, 0);
    if (reader.fd == -1) {
        return std::nullopt;
    }
    struct stat st{};
    if (fstat(reader.fd, &st) == -1) {
        ::close(reader.fd);
        return std::nullopt;
    }
    reader.size = static_cast<std::size_t>(st.st_size);
    void* addr = ::mmap(nullptr, reader.size, PROT_READ, MAP_SHARED, reader.fd, 0);
    if (addr == MAP_FAILED) {
        ::close(reader.fd);
        return std::nullopt;
    }
    reader.ptr = static_cast<snapshot::SharedSnapshot*>(addr);
    return reader;
}

bool readSharedSnapshot(SharedSnapshotReader& reader,
                        InstrumentToken token,
                        Snapshot& snapshot) {
    if (!reader.ptr) return false;
    const auto seq = reader.ptr->sequence.load(std::memory_order_acquire);
    if (seq == reader.last_seq) {
        return false;
    }
    reader.last_seq = seq;

    snapshot.token = token;
    snapshot.instrument = "Token " + std::to_string(token);
    snapshot.timestamp = formatTimestamp(reader.ptr->timestamp_ns);
    snapshot.ltp = reader.ptr->ltp;
    snapshot.ltq = reader.ptr->ltq;
    snapshot.bids.clear();
    snapshot.asks.clear();
    const auto maxLevels = reader.ptr->max_levels;
    const auto bidCount = std::min(reader.ptr->bid_count, maxLevels);
    const auto askCount = std::min(reader.ptr->ask_count, maxLevels);
    const snapshot::Level* bidSrc = snapshot::bidLevels(reader.ptr);
    const snapshot::Level* askSrc = snapshot::askLevels(reader.ptr);
    for (uint32_t i = 0; i < bidCount; ++i) {
        snapshot.bids.push_back({bidSrc[i].price, bidSrc[i].qty});
    }
    for (uint32_t i = 0; i < askCount; ++i) {
        snapshot.asks.push_back({askSrc[i].price, askSrc[i].qty});
    }
    return true;
}

std::string priceKey(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4) << value;
    return out.str();
}

struct LevelAnimator {
    std::unordered_map<std::string, double> values;

    double smooth(const std::string& key, double target) {
        constexpr double kSmoothing = 0.35;
        double current = target;
        auto it = values.find(key);
        if (it != values.end()) {
            current = it->second;
        }
        double next = current + (target - current) * kSmoothing;
        values[key] = next;
        return next;
    }

    void prune(const std::unordered_set<std::string>& active) {
        for (auto it = values.begin(); it != values.end();) {
            if (active.find(it->first) == active.end()) {
                it = values.erase(it);
            } else {
                ++it;
            }
        }
    }
};

LevelAnimator gBidAnimator;
LevelAnimator gAskAnimator;

chtype colorAttr(int pairId) {
    return gColorsEnabled ? COLOR_PAIR(pairId) : static_cast<chtype>(0);
}

void drawCenteredText(int row, const std::string& text, int cols, chtype attr = A_NORMAL) {
    const int start = std::max(0, (cols - static_cast<int>(text.size())) / 2);
    if (attr != A_NORMAL) {
        attron(attr);
    }
    mvprintw(row, start, "%s", text.c_str());
    if (attr != A_NORMAL) {
        attroff(attr);
    }
}

std::string truncateMiddle(const std::string& text, int maxWidth) {
    if (maxWidth <= 0) {
        return {};
    }
    const auto width = static_cast<std::string::size_type>(maxWidth);
    if (text.size() <= width) {
        return text;
    }
    if (width <= 3) {
        const auto suffix = std::min(text.size(), width);
        return text.substr(text.size() - suffix);
    }
    const int room = static_cast<int>(width) - 3;
    const auto head = static_cast<std::string::size_type>(room / 2);
    const auto tail = static_cast<std::string::size_type>(room - static_cast<int>(head));
    return text.substr(0, head) + "..." + text.substr(text.size() - tail);
}

void cleanupFlashes(FlashMap& flashes, Clock::time_point now) {
    for (auto it = flashes.begin(); it != flashes.end();) {
        if (it->second <= now) {
            it = flashes.erase(it);
        } else {
            ++it;
        }
    }
}

void registerFlashes(const std::vector<Level>& prev,
                     const std::vector<Level>& next,
                     FlashMap& flashes,
                     Clock::time_point now) {
    std::unordered_map<std::string, double> prevMap;
    prevMap.reserve(prev.size());
    for (const auto& lvl : prev) {
        prevMap[priceKey(lvl.price)] = lvl.qty;
    }

    for (const auto& lvl : next) {
        const auto key = priceKey(lvl.price);
        const auto it = prevMap.find(key);
        const double prevQty = (it != prevMap.end()) ? it->second : -1.0;
        if (prevQty < 0 || std::fabs(prevQty - lvl.qty) > 0.0001) {
            flashes[key] = now + kFlashDuration;
        }
    }
}

bool isFlashing(const FlashMap& flashes,
                const std::string& key,
                Clock::time_point now) {
    const auto it = flashes.find(key);
    return it != flashes.end() && it->second > now;
}

void drawDepthBar(int row,
                  int startCol,
                  int width,
                  double ratio,
                  bool isBid,
                  int cols,
                  int colorPair) {
    if (width <= 0 || ratio <= 0.0) {
        return;
    }
    const int fill = std::clamp(static_cast<int>(std::round(ratio * width)), 0, width);
    const chtype base = colorAttr(colorPair);
    if (base == 0) {
        return;
    }
    const chtype attr = base | A_DIM;
    for (int i = 0; i < fill; ++i) {
        const int col = isBid ? (startCol - i) : (startCol + i);
        if (col >= 0 && col < cols) {
            mvaddch(row, col, ' ' | attr);
        }
    }
}

void renderSnapshot(const Snapshot& snapshot,
                    bool hasData,
                    const std::string& status,
                    const std::string& sourcePath,
                    const FlashMap& bidFlashes,
                    const FlashMap& askFlashes,
                    Clock::time_point now) {
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);

    erase();

    drawCenteredText(0, "SIMEX ORDERBOOK (AUTHOR: SOUVIK MUKHERJEE)", cols, colorAttr(kPairHeader) | A_BOLD);

    if (rows < 20 || cols < 90) {
        drawCenteredText(rows / 2, "Enlarge the terminal for the orderbook view", cols, A_BOLD);
        refresh();
        return;
    }

    if (!hasData) {
        drawCenteredText(rows / 2, status, cols, A_BOLD);
        const std::string watch = "Watching " + sourcePath;
        mvprintw(rows - 2, 2, "%s", watch.c_str());
        mvprintw(rows - 1, 2, "Press q to quit");
        refresh();
        return;
    }

    std::string nameLine = snapshot.instrument.empty()
        ? ("Token " + std::to_string(snapshot.token))
        : (snapshot.instrument + " (" + std::to_string(snapshot.token) + ")");
    mvprintw(1, 2, "%s", nameLine.c_str());

    const std::string headline = formatPrice(snapshot.ltp);
    drawCenteredText(2, headline, cols, colorAttr(kPairHeadline) | A_BOLD);

    mvprintw(3, 2, "%s", status.c_str());
    const std::string meta = "Source " + truncateMiddle(sourcePath, cols - 20);
    mvprintw(3, cols - static_cast<int>(meta.size()) - 2, "%s", meta.c_str());

    const int contentTop = 5;
    const int contentBottom = rows - 5;
    const int centerRow = (contentTop + contentBottom) / 2;
    const int levelsAvailableTop = std::max(0, centerRow - contentTop);
    const int levelsAvailableBottom = std::max(0, contentBottom - centerRow);
    const int levelsPerSide = std::min({kDisplayLevels, levelsAvailableTop, levelsAvailableBottom});
    const int centerCol = cols / 2;
    const int dividerTop = contentTop - 1;

    mvvline(dividerTop, centerCol, ACS_VLINE, contentBottom - dividerTop + 1);

    const int priceWidth = 10;
    const int qtyWidth = 7;
    const int leftMargin = 4;
    const int rightMargin = cols - 4;
    const int centerGap = 6;

    const int bidPriceCol = leftMargin;
    const int bidQtyCol = bidPriceCol + priceWidth + 2;
    const int bidBarStart = bidQtyCol + qtyWidth + 2;
    const int bidBarEnd = centerCol - centerGap;
    const int bidBarWidth = std::max(0, bidBarEnd - bidBarStart);

    const int askPriceCol = rightMargin - priceWidth;
    const int askQtyCol = askPriceCol - qtyWidth - 2;
    const int askBarEnd = askQtyCol - 2;
    const int askBarStart = centerCol + centerGap;
    const int askBarWidth = std::max(0, askBarEnd - askBarStart);

    auto depthMax = 0.0;
    for (int i = 0; i < levelsPerSide && i < static_cast<int>(snapshot.bids.size()); ++i) {
        depthMax = std::max(depthMax, snapshot.bids[static_cast<std::size_t>(i)].qty);
    }
    for (int i = 0; i < levelsPerSide && i < static_cast<int>(snapshot.asks.size()); ++i) {
        depthMax = std::max(depthMax, snapshot.asks[static_cast<std::size_t>(i)].qty);
    }
    if (depthMax <= 0) {
        depthMax = 1.0;
    }

    std::unordered_set<std::string> activeBidKeys;
    std::unordered_set<std::string> activeAskKeys;

    auto drawLevel = [&](const Level* level,
                         bool isBid,
                         bool isBest,
                         int row) {
        const int priceCol = isBid ? bidPriceCol : askPriceCol;
        const int qtyCol = isBid ? bidQtyCol : askQtyCol;

        if (!level) {
            mvprintw(row, priceCol, "%*s", priceWidth, "");
            mvprintw(row, qtyCol, "%*s", qtyWidth, "");
            return;
        }

        const std::string key = priceKey(level->price);
        if (isBid) {
            activeBidKeys.insert(key);
        } else {
            activeAskKeys.insert(key);
        }

        const double rawRatio = std::clamp(level->qty / depthMax, 0.0, 1.0);
        const double ratio = (isBid ? gBidAnimator : gAskAnimator).smooth(key, rawRatio);
        if (isBid) {
            drawDepthBar(row, bidBarStart, bidBarWidth, ratio, false, cols, kPairBidDepth);
        } else {
            drawDepthBar(row, askBarEnd, askBarWidth, ratio, true, cols, kPairAskDepth);
        }

        const std::string price = formatPrice(level->price);
        const std::string qty = formatQty(level->qty);
        const bool flash = isBid
            ? isFlashing(bidFlashes, key, now)
            : isFlashing(askFlashes, key, now);

        const chtype attr = colorAttr(isBid ? kPairBidText : kPairAskText)
            | (flash ? A_BOLD : A_NORMAL)
            | (isBest ? A_STANDOUT : A_NORMAL);
        attron(attr);
        if (isBid) {
            mvprintw(row, priceCol, "%*s", priceWidth, price.c_str());
            mvprintw(row, qtyCol, "%*s", qtyWidth, qty.c_str());
        } else {
            mvprintw(row, qtyCol, "%*s", qtyWidth, qty.c_str());
            mvprintw(row, priceCol, "%-*s", priceWidth, price.c_str());
        }
        attroff(attr);
    };

    for (int i = 0; i < levelsPerSide; ++i) {
        const int askIdx = i;
        const int askRow = centerRow - 1 - i;
        const Level* askLevel = (askIdx < static_cast<int>(snapshot.asks.size()))
            ? &snapshot.asks[static_cast<std::size_t>(askIdx)]
            : nullptr;
        drawLevel(askLevel, false, i == 0, askRow);
    }

    for (int i = 0; i < levelsPerSide; ++i) {
        const int bidIdx = i;
        const int bidRow = centerRow + i;
        const Level* bidLevel = (bidIdx < static_cast<int>(snapshot.bids.size()))
            ? &snapshot.bids[static_cast<std::size_t>(bidIdx)]
            : nullptr;
        drawLevel(bidLevel, true, i == 0, bidRow);
    }

    gBidAnimator.prune(activeBidKeys);
    gAskAnimator.prune(activeAskKeys);

    const std::string ltpLine =
        "LTP " + formatPrice(snapshot.ltp) + "   LTQ " + formatQty(snapshot.ltq);
    drawCenteredText(rows - 3, ltpLine, cols, colorAttr(kPairFooter) | A_BOLD);

    const std::string tsLine = "Last update " + snapshot.timestamp;
    drawCenteredText(rows - 2, tsLine, cols, A_DIM);
    drawCenteredText(rows - 1, "Press q to quit", cols, A_DIM);

    refresh();
}

class NcursesSession {
public:
    NcursesSession() {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
        curs_set(0);
        gColorsEnabled = has_colors();
        if (gColorsEnabled) {
            start_color();
            use_default_colors();

            short bidBg = COLOR_BLACK;
            short askBg = COLOR_BLACK;
            short headline = COLOR_MAGENTA;

            if (can_change_color()) {
                init_color(kColorBidBg, 100, 250, 100);
                init_color(kColorAskBg, 350, 100, 100);
                init_color(kColorHeadline, 800, 200, 500);
                bidBg = kColorBidBg;
                askBg = kColorAskBg;
                headline = kColorHeadline;
            }

            init_pair(kPairBidText, COLOR_GREEN, bidBg);
            init_pair(kPairAskText, COLOR_RED, askBg);
            init_pair(kPairHeader, COLOR_CYAN, -1);
            init_pair(kPairFooter, COLOR_YELLOW, -1);
            init_pair(kPairBidDepth, bidBg, bidBg);
            init_pair(kPairAskDepth, askBg, askBg);
            init_pair(kPairHeadline, headline, -1);
        }
    }

    ~NcursesSession() {
        endwin();
    }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <instrument-token>\n", argv[0]);
        return 1;
    }

    InstrumentToken token = 0;
    try {
        token = static_cast<InstrumentToken>(std::stoul(argv[1]));
    } catch (const std::exception&) {
        std::fprintf(stderr, "Invalid instrument token: %s\n", argv[1]);
        return 1;
    }

    std::string prefix = "/simex_book";
    if (argc >= 3) {
        prefix = argv[2];
    }

    auto readerOpt = mapSharedSnapshot(prefix, token);
    if (!readerOpt) {
        std::fprintf(stderr, "Failed to map shared snapshot for token %u\n", token);
        return 1;
    }
    SharedSnapshotReader reader = *readerOpt;
    const std::string sourceLabel = "shm " + shmRegionName(prefix, token);

    NcursesSession session;

    bool hasData = false;
    Snapshot snapshot;
    Snapshot previous;
    bool hasPrevious = false;
    FlashMap bidFlashes;
    FlashMap askFlashes;
    std::string status = "Waiting for " + sourceLabel;

    while (true) {
        const auto loopTime = Clock::now();
        if (readSharedSnapshot(reader, token, snapshot)) {
            if (hasPrevious) {
                registerFlashes(previous.bids, snapshot.bids, bidFlashes, loopTime);
                registerFlashes(previous.asks, snapshot.asks, askFlashes, loopTime);
            }
            previous = snapshot;
            hasPrevious = hasData;
            hasData = true;
            status = "Updated " + snapshot.timestamp;
        } else if (!hasData) {
            status = "Waiting for " + sourceLabel;
        }

        cleanupFlashes(bidFlashes, loopTime);
        cleanupFlashes(askFlashes, loopTime);
        renderSnapshot(snapshot, hasData, status, sourceLabel, bidFlashes, askFlashes, loopTime);

        const int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            break;
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    if (reader.ptr && reader.ptr != MAP_FAILED) {
        ::munmap(reader.ptr, reader.size);
    }
    if (reader.fd != -1) {
        ::close(reader.fd);
    }

    return 0;
}
