#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SECONDS_PER_DAY (24 * 60 * 60)
#define DAYS_PER_400_YEARS (365 * 400 + 97)
#define DAYS_PER_100_YEARS (365 * 100 + 24)
#define DAYS_PER_4_YEARS (365 * 4 + 1)

static int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct datetime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int nsec;
};

struct string_builder {
    char buf[64];
    char *p;
};

int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

struct string_builder* sb_init(struct string_builder *sb) {
    sb->p = sb->buf;
    return sb;
}

void sb_append_char(struct string_builder *sb, char c) {
    *sb->p++ = c;
}

void sb_append_padded(struct string_builder *sb, int value, int width) {
    int divisor = 1;
    for (int i = 1; i < width; i++) {
        divisor *= 10;
    }
    for (int i = 0; i < width; i++) {
        *sb->p++ = (value / divisor) % 10 + '0';
        divisor /= 10;
    }
}

char* sb_build(struct string_builder *sb) {
    *sb->p = '\0';
    return sb->buf;
}

void timestamp_to_datetime(long ns, struct datetime *dt) {
    long seconds = ns / 1000000000LL;
    dt->nsec = ns % 1000000000LL;
    
    long sec_in_day = seconds % SECONDS_PER_DAY;
    dt->hour = sec_in_day / 3600;
    dt->minute = (sec_in_day % 3600) / 60;
    dt->second = sec_in_day % 60;
    
    long days_since_epoch = seconds / SECONDS_PER_DAY;
    
    dt->year = 1970;
    
    long cycles_400 = days_since_epoch / DAYS_PER_400_YEARS;
    dt->year += cycles_400 * 400;
    days_since_epoch %= DAYS_PER_400_YEARS;
    
    long cycles_100 = days_since_epoch / DAYS_PER_100_YEARS;
    if (cycles_100 > 3) cycles_100 = 3;
    dt->year += cycles_100 * 100;
    days_since_epoch -= cycles_100 * DAYS_PER_100_YEARS;
    
    long cycles_4 = days_since_epoch / DAYS_PER_4_YEARS;
    dt->year += cycles_4 * 4;
    days_since_epoch %= DAYS_PER_4_YEARS;
    
    int y;
    for (y = 0; y < 4 && days_since_epoch >= (is_leap_year(dt->year + y) ? 366 : 365); y++) {
        days_since_epoch -= (is_leap_year(dt->year + y) ? 366 : 365);
    }
    dt->year += y;
    
    dt->month = 1;
    int days_in_feb = is_leap_year(dt->year) ? 29 : 28;
    int *days_table = days_per_month;
    
    int d;
    for (d = 0; d < 12 && days_since_epoch >= days_table[d]; d++) {
        if (d == 1) {
            days_since_epoch -= days_in_feb;
        } else {
            days_since_epoch -= days_table[d];
        }
    }
    
    dt->month = d + 1;
    dt->day = days_since_epoch + 1;
}

void datetime_print(struct datetime *dt) {
    struct string_builder sb;
    
    sb_init(&sb);
    sb_append_padded(&sb, dt->year, 4);
    sb_append_char(&sb, '-');
    sb_append_padded(&sb, dt->month, 2);
    sb_append_char(&sb, '-');
    sb_append_padded(&sb, dt->day, 2);
    sb_append_char(&sb, ' ');
    sb_append_padded(&sb, dt->hour, 2);
    sb_append_char(&sb, ':');
    sb_append_padded(&sb, dt->minute, 2);
    sb_append_char(&sb, ':');
    sb_append_padded(&sb, dt->second, 2);
    sb_append_char(&sb, '.');
    sb_append_padded(&sb, dt->nsec, 9);
    
    printf("%s\n", sb_build(&sb));
}

int
main(int argc, char *argv[])
{
    struct datetime dt;
    timestamp_to_datetime(gettime(), &dt);
    datetime_print(&dt);
    exit(0);
}
