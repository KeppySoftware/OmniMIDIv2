#ifndef SAMPLERATESELECTOR_H
#define SAMPLERATESELECTOR_H

#include <QComboBox>

#define RATES_COUNT 23

class SampleRateSelector : public QComboBox
{
    Q_OBJECT
public:
    explicit SampleRateSelector(QWidget *parent = nullptr);
    int getRate();
    void setRate(int rate);

private:
    const int rates[RATES_COUNT] = {
        384000,
        352800,
        192000,
        176400,
        96000,
        88200,
        74750,
        66150,
        64000,
        50400,
        50000,
        48000,
        47250,
        44100,
        44056,
        37800,
        34750,
        32000,
        22050,
        16000,
        11025,
        8000,
        4000
    };

signals:
};

#endif // SAMPLERATESELECTOR_H
