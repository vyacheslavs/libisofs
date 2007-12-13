/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "util.h"

int div_up(int n, int div)
{
    return (n + div - 1) / div;
}

int round_up(int n, int mul)
{
    return div_up(n, mul) * mul;
}
