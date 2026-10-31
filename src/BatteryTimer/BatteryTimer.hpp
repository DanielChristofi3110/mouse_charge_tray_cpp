#pragma once
#include <string>

//formats minutes(double) to wstring
std::wstring FormatMinutes(double minutes);


//gets current time
int64_t GetCurrentTimeMs();


//estimates time to dis wstring
std::wstring EstimateTime(int current);
