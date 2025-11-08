import csv
import math
import random
import datetime
from pathlib import Path

INPUT_FILE = "market_data.csv"
OUTPUT_FILE = "trades.csv"
TRADES_PER_SYMBOL = 200        # per instrument
SIGMA = 0.005                  # low variance (≈0.5%)
PRICE_DEVIATION_CLAMP = 0.05   # clamp ±5%

def round_to_tick(price: float, tick_size: int) -> int:
    if tick_size <= 0:
        return int(round(price))
    return int(round(price / tick_size)) * tick_size


def generate_trades_for_instrument(inst, rng):
    trades = []
    token = int(inst["token"])
    symbol = inst["symbol"]
    lot_size = int(inst["lot_size"])
    tick_size = int(inst["tick_size"])
    prev_close = int(inst["underlying_prev_close"])

    m = math.log(max(prev_close, 1)) - (SIGMA ** 2) / 2.0

    time_now = datetime.datetime.utcnow()

    for i in range(TRADES_PER_SYMBOL):
        price = rng.lognormvariate(m, SIGMA)
        price = round_to_tick(price, tick_size)

        lo = int(prev_close * (1.0 - PRICE_DEVIATION_CLAMP))
        hi = int(prev_close * (1.0 + PRICE_DEVIATION_CLAMP))
        price = max(lo, min(hi, price))
        price = round_to_tick(price, tick_size)

        qty_mult = rng.randint(1, 5)
        qty = lot_size * qty_mult

        bias = 0.5 + ((price - prev_close) / prev_close) * 5
        bias = max(0.45, min(0.55, bias))
        side = "BUY" if rng.random() < bias else "SELL"

        ts = (time_now + datetime.timedelta(milliseconds=i * 2)).isoformat(timespec="milliseconds") + "Z"

        trades.append({
            "timestamp": ts,
            "token": token,
            "side": side,
            "qty": qty,
            "price": price
        })
    return trades


def main():
    input_path = Path(INPUT_FILE)
    output_path = Path(OUTPUT_FILE)

    if not input_path.exists():
        print(f"❌ Input file not found: {INPUT_FILE}")
        return

    instruments = []
    with open(input_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            instruments.append(row)

    rng = random.Random()

    all_trades = []
    for inst in instruments:
        trades = generate_trades_for_instrument(inst, rng)
        all_trades.extend(trades)
        print(f"✅ Generated {len(trades)} trades for {inst['symbol']}")

    with open(output_path, "w", newline="") as f:
        fieldnames = ["timestamp", "token", "side", "qty", "price"]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(all_trades)

    print(f"\n💾 Wrote {len(all_trades)} trades to {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
