/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "ns.h"
#include <math.h>

int au915_get_ch(float MHz, uint8_t DataRate)
{
    float i;    /* x10: in 100's of Khz */
    modff(MHz*10, &i);

    if (DataRate == 4)
        return ((i - 9159) / 16) + 64;
    else if (DataRate < 4)
        return (i - 9152) / 2;
    else
        return -1;
}

/*!
 * Up/Down link data rates offset definition
 */
static const int8_t drOffsets[7][6] =
{ /* RX1DROffset:     0      1      2      3     4       5 */
    /* DR_0 */    { DR_8 , DR_8 , DR_8 , DR_8 , DR_8 , DR_8 },
    /* DR_1 */    { DR_9 , DR_8 , DR_8 , DR_8 , DR_8 , DR_8 },
    /* DR_2 */    { DR_10, DR_9 , DR_8 , DR_9 , DR_8 , DR_8 },
    /* DR_3 */    { DR_11, DR_10, DR_9 , DR_8 , DR_8 , DR_8 },
    /* DR_4 */    { DR_12, DR_11, DR_10, DR_9 , DR_8 , DR_8 },
    /* DR_5 */    { DR_13, DR_12, DR_11, DR_10, DR_9 , DR_8 },
    /* DR_6 */    { DR_13, DR_13, DR_12, DR_11, DR_10, DR_9 }
};

void
rx1_band_conv_au915(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out)
{
    uint8_t down_channel = 0xff;

    out->DLFreq2 = 0;
    if (ULDataRate == 6)
        down_channel = au915_get_ch(ULFreq, ULDataRate) - 64;
    else if (ULDataRate < 6) {
        uint8_t up_channel = au915_get_ch(ULFreq, ULDataRate);
        down_channel = up_channel & 7;
    }
    out->DLFreq1 = 923.3 + (0.6 * down_channel);
    out->DataRate1 = drOffsets[ULDataRate][rxdrOffset1];
}

