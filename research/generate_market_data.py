import csv
from nubra_python_sdk.refdata.instruments import InstrumentData
from nubra_python_sdk.start_sdk import InitNubraSdk, NubraEnv

nubra = InitNubraSdk(NubraEnv.PROD)
instruments = InstrumentData(nubra)

symbols = ["HDFCBANK", "RELIANCE", "TCS", "INFY", "ICICIBANK"]
exchange = "NSE"

output_file = "market_data.csv"

with open(output_file, "w", newline="") as csvfile:
    fieldnames = ["symbol", "token", "stock_name", "lot_size", "tick_size", "underlying_prev_close"]
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()

    for symbol in symbols:
        try:
            instrument = instruments.get_instrument_by_symbol(symbol, exchange=exchange)

            row = {
                "symbol": symbol,
                "token": instrument.token,
                "stock_name": instrument.stock_name,
                "lot_size": instrument.lot_size,
                "tick_size": instrument.tick_size,
                "underlying_prev_close": instrument.underlying_prev_close
            }

            writer.writerow(row)
            print(f"✅ Added {symbol} to CSV")

        except Exception as e:
            print(f"❌ Failed to fetch {symbol}: {e}")

print(f"\n💾 CSV successfully written to: {output_file}")
