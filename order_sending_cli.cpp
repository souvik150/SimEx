#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <string>

#include "ingress/McastSocket.h"
#include "ingress/WireOrder.h"
#include "types/AppTypes.h"
#include "utils/Config.h"

#ifdef PROJECT_ROOT
constexpr const char* kConfigPath = PROJECT_ROOT "/config/app.ini";
#else
constexpr const char* kConfigPath = "config/app.ini";
#endif

namespace {

std::string trim(const std::string& input) {
    const auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

bool promptLine(const std::string& label, std::string& out) {
    while (true) {
        std::cout << label << " (or 'q' to quit): ";
        if (!std::getline(std::cin, out)) {
            return false;
        }
        out = trim(out);
        if (out.empty()) {
            std::cout << "Please provide a value.\n";
            continue;
        }
        if (out == "q" || out == "Q" || out == "quit" || out == "QUIT") {
            return false;
        }
        return true;
    }
}

template <typename Parser>
bool promptValue(const std::string& label, Parser parser) {
    std::string input;
    while (promptLine(label, input)) {
        if (parser(input)) {
            return true;
        }
        std::cout << "Invalid input. Try again.\n";
    }
    return false;
}

bool parseOrderId(const std::string& text, OrderId& out) {
    try {
        out = static_cast<OrderId>(std::stoull(text));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseInstrument(const std::string& text, InstrumentToken& out) {
    try {
        out = static_cast<InstrumentToken>(std::stoul(text));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseQty(const std::string& text, Qty& out) {
    try {
        out = static_cast<Qty>(std::stoul(text));
        return out > 0;
    } catch (const std::exception&) {
        return false;
    }
}

bool parsePrice(const std::string& text, Price& out) {
    try {
        out = static_cast<Price>(std::stoull(text));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace

int main() {
    try {
        const AppConfig config = loadConfig(kConfigPath);
        SocketUtils::McastSocket socket;
        socket.init(config.mcast_ip, config.mcast_iface, config.mcast_port, false);

        std::cout << "Manual Order CLI ready for " << config.mcast_ip << ":" << config.mcast_port
                  << " via iface '" << config.mcast_iface << "'.\n";
        std::cout << "Enter order details below. Type 'q' to exit.\n\n";

        while (true) {
            ingress::WireOrder order{};

            if (!promptValue("Order ID", [&](const std::string& input) {
                    return parseOrderId(input, order.order_id);
                })) {
                break;
            }

            if (!promptValue("Instrument token", [&](const std::string& input) {
                    return parseInstrument(input, order.instrument);
                })) {
                break;
            }

            if (!promptValue("Side (BUY/SELL)", [&](const std::string& input) {
                    const auto maybe = ingress::sideFromString(input);
                    if (!maybe) {
                        return false;
                    }
                    order.side = *maybe;
                    return true;
                })) {
                break;
            }

            if (!promptValue("Quantity", [&](const std::string& input) {
                    return parseQty(input, order.quantity);
                })) {
                break;
            }

            if (!promptValue("Price (0 for MARKET)", [&](const std::string& input) {
                    return parsePrice(input, order.price);
                })) {
                break;
            }

            if (!promptValue("Order type (LIMIT/MARKET/IOC/FOK/ICEBERG)", [&](const std::string& input) {
                    const auto maybe = ingress::orderTypeFromString(input);
                    if (!maybe) {
                        return false;
                    }
                    order.type = *maybe;
                    return true;
                })) {
                break;
            }

            order.display = 0;
            if (order.type == OrderType::ICEBERG) {
                if (!promptValue("Display quantity", [&](const std::string& input) {
                        return parseQty(input, order.display);
                    })) {
                    break;
                }
            }

            const std::string payload = ingress::serializeWireOrder(order);
            socket.send(payload.data(), payload.size());
            socket.sendAndRecv();

            std::cout << "Sent order: " << payload << "\n\n";
        }

        std::cout << "Exiting manual order CLI.\n";
    } catch (const std::exception& ex) {
        std::cerr << "CLI error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
