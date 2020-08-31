#ifndef __MAIN_H__
#define __MAIN_H__


struct TriggerSettings
{
    int     start_hour;
    int     start_minute;
    int     stop_hour;
    int     stop_minute;
    bool    start_trigger_on{true};
    bool    stop_trigger_on{true};
    bool    week_days[7];
};


void    save_trigger_settings();
void    load_trigger_settings();


#endif // __MAIN_H__