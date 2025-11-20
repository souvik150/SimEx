#pragma once

#include "types/AppTypes.h"
#include "types/OrderSide.h"

struct TradeEvent {
    InstrumentToken instrument;
    Side aggressorSide;
    OrderId aggressorId;
    Side restingSide;
    OrderId restingOrderId;
    Price price;
    Qty quantity;
};
