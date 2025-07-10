#include "samplerateselector.h"

SampleRateSelector::SampleRateSelector(QWidget *parent)
    : QComboBox{parent}
{
    addItem("Default", 0);
    for (int rate : rates) {
        addItem(QString::number(rate), rate);
    }
}

int SampleRateSelector::getRate() {
    if (!currentData().isNull())
        return currentData().toInt();
    else
        return 0;
}

void SampleRateSelector::setRate(int rate) {
    if (rate == 0) {
        setCurrentIndex(0);
        return;
    }

    // assign the value which is closest to the given rate
    int closest_idx = -1;
    int closest_diff = INT_MAX;
    int curr_diff = 0;
    for (int i = 0; i < RATES_COUNT; ++i) {
        curr_diff = abs(rates[i] - rate);
        if (curr_diff < closest_diff) {
            closest_diff = curr_diff;
            closest_idx = i;
        }
    }
    // +1 because of the first "default" value
    setCurrentIndex(closest_idx + 1);
}
