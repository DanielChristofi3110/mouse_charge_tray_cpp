#include "BatteryTimer.hpp"
#include <chrono>
#include <spdlog/spdlog.h>


static int lastBattery = -1;
static int64_t lastTimestamp = -1;
static double rateMinutesPerPercent = -1.0;
static std::string lastDirection = "none";

std::wstring FormatMinutes(double minutes)
{
    if (minutes < 1.0) return L"<1m";
    int m = static_cast<int>(minutes);
    int h = m / 60;
    int min = m % 60;

    if (h > 0) return std::to_wstring(h) + L"h " + std::to_wstring(min) + L"m";
    return std::to_wstring(min) + L"m";
}

int64_t GetCurrentTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

std::wstring EstimateTime(int current)
{
    int64_t now = GetCurrentTimeMs();

    if (lastBattery == -1)
    {
        lastBattery = current;
        lastTimestamp = now;
        spdlog::warn("Calculating Time lastBattery:{} lastTimestamp:{}",lastBattery,lastTimestamp);
        return L"Calculating…";
    }

    if (current == lastBattery)
    {

        spdlog::info("Calculating Time lastBattery:{} lastTimestamp:{}",lastBattery,lastTimestamp);
        if (rateMinutesPerPercent <= 0) return L"Calculating…";
        if (lastDirection == "down")
            return L"Discharge: " + FormatMinutes(current * rateMinutesPerPercent);
        else if (lastDirection == "up")
            return L"Charge: " + FormatMinutes((100 - current) * rateMinutesPerPercent);
        else
            return L"Calculating…";
    }

    int64_t deltaMs = now - lastTimestamp;
    if (deltaMs <= 0) deltaMs = 1;
    double deltaMinutes = static_cast<double>(deltaMs) / 60000.0;
    int deltaPercent = current - lastBattery;

    std::string direction = (deltaPercent < 0) ? "down" : "up";

    if (std::abs(deltaPercent) > 10)
    {
        spdlog::info("Battery jump detected -> reset tracking");
        lastBattery = current;
        lastTimestamp = now;
        return L"Calculating…";
    }

    double minutesPerPercent = deltaMinutes / std::abs(deltaPercent);

    if (rateMinutesPerPercent < 0)
        rateMinutesPerPercent = minutesPerPercent;
    else
        rateMinutesPerPercent = (rateMinutesPerPercent * 0.7) + (minutesPerPercent * 0.3);


    spdlog::info("rateMinutesPerPercent:{}",rateMinutesPerPercent);
    lastDirection = direction;
    lastBattery = current;
    lastTimestamp = now;

    if (direction == "down")
    {
        double remaining = current * rateMinutesPerPercent;

        spdlog::info("remaining:{}",remaining);
        return L"Discharge: " + FormatMinutes(remaining);
    }
    else
    {
        double remaining = (100 - current) * rateMinutesPerPercent;
        spdlog::info("remaining:{}",remaining);
        return L"Charge: " + FormatMinutes(remaining);
    }
}
