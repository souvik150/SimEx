import pandas as pd
import heapq

INPUT_FILE = "trades.csv"
OUTPUT_FILE = "trade_summary.csv"

df = pd.read_csv(INPUT_FILE)
df = df.sort_values(by=["timestamp"])

summary = []

for token, group in df.groupby("token"):
    buy_book = []
    sell_book = []

    trades_executed = 0
    traded_qty = 0

    for _, row in group.iterrows():
        side = row["side"]
        price = int(row["price"])
        qty = int(row["qty"])

        if side == "BUY":
            while qty > 0 and sell_book and sell_book[0][0] <= price:
                sell_price, sell_qty = heapq.heappop(sell_book)
                traded = min(qty, sell_qty)
                trades_executed += 1
                traded_qty += traded
                qty -= traded
                sell_qty -= traded
                if sell_qty > 0:
                    heapq.heappush(sell_book, (sell_price, sell_qty))
            if qty > 0:
                heapq.heappush(buy_book, (-price, qty))

        elif side == "SELL":
            while qty > 0 and buy_book and -buy_book[0][0] >= price:
                buy_price, buy_qty = heapq.heappop(buy_book)
                buy_price = -buy_price
                traded = min(qty, buy_qty)
                trades_executed += 1
                traded_qty += traded
                qty -= traded
                buy_qty -= traded
                if buy_qty > 0:
                    heapq.heappush(buy_book, (-buy_price, buy_qty))
            if qty > 0:
                heapq.heappush(sell_book, (price, qty))

    summary.append({
        "token": token,
        "trades_executed": trades_executed,
        "total_traded_qty": traded_qty
    })

summary_df = pd.DataFrame(summary)
summary_df.to_csv(OUTPUT_FILE, index=False)

print(summary_df)
print(f"\n💾 Summary written to: {OUTPUT_FILE}")
