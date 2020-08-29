#ifndef __MAIN_H__
#define __MAIN_H__


struct TriggerSettings
{
    int     start_hour;
    int     start_minute;
    int     stop_hour;
    int     stop_minute;
    bool    start_trigger_on{true};
    bool    stop_trigger_on{false};
    bool    week_days[7];
    bool    all_week{true};
};


#endif // __MAIN_H__