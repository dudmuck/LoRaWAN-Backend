/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "sNS.h"

static const int8_t drOffsets[8][6] =
{ /*  RX1DROffset:     0,    1,    2,    3,    4,    5 */
    /* up DR_0 */ { DR_0, DR_0, DR_0, DR_0, DR_0, DR_0 },
    /* up DR_1 */ { DR_1, DR_0, DR_0, DR_0, DR_0, DR_0 },
    /* up DR_2 */ { DR_2, DR_1, DR_0, DR_0, DR_0, DR_0 },
    /* up DR_3 */ { DR_3, DR_2, DR_1, DR_0, DR_0, DR_0 },
    /* up DR_4 */ { DR_4, DR_3, DR_2, DR_1, DR_0, DR_0 },
    /* up DR_5 */ { DR_5, DR_4, DR_3, DR_2, DR_1, DR_0 },
    /* up DR_6 */ { DR_6, DR_5, DR_4, DR_3, DR_2, DR_1 },
    /* up DR_7 */ { DR_7, DR_6, DR_5, DR_4, DR_3, DR_2 }
};

void rx1_band_conv_eu868(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out)
{
    out->DLFreq2 = 0;
    out->DLFreq1 = ULFreq;
    out->DataRate1 = drOffsets[ULDataRate][rxdrOffset1];
}
