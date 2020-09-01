#ifndef __MAIN_H__
#define __MAIN_H__


struct TriggerSettings
{
    int     start_hour;
    int     start_minute;
    int     stop_hour;
    int     stop_minute;
    bool    trigger_on{true};
    bool    week_days[7];
};


void    save_trigger_settings();
void    load_trigger_settings();
bool    should_open_advanced();
bool    should_open();
void    reset_state();


#endif // __MAIN_H__