#include "battery.h"
/* Approximate rested CR2032 usable charge, not a manufacturer SOC curve.
 * Output uses ZCL half-percent units; 2.2 V is the SDK flash safety limit. */
uint8_t sensor_batteryPercent(uint16_t mv)
{
    static const struct {uint16_t mv; uint8_t halfPercent;} points[] = {
        {2200,0}, {2500,10}, {2600,20}, {2700,40},
        {2800,90}, {2900,140}, {3000,180}, {3100,200},
    };
    if (mv <= points[0].mv) return 0;
    for (unsigned i=1; i<sizeof(points)/sizeof(points[0]); ++i) {
        if (mv <= points[i].mv) {
            unsigned span=points[i].mv-points[i-1].mv;
            unsigned rise=points[i].halfPercent-points[i-1].halfPercent;
            return points[i-1].halfPercent + ((mv-points[i-1].mv)*rise+span/2)/span;
        }
    }
    return 200;
}
