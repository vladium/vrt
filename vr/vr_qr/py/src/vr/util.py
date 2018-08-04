
import datetime as dt

import dateutil.parser

from .classes import is_integral

# -------------------------------------------------------------------------------------------------

_DUMMY_DATE     = dt.date (1000, 6, 15)

# .................................................................................................

def as_date (x):
    if isinstance (x, dt.date): return x
    
    if isinstance (x, str):
        return dateutil.parser.parse (x).date ()
    elif is_integral (x):
        return dateutil.parser.parse (str (x)).date ()
    raise ValueError ("can't convert value of type %s to date" % type (x))

def as_idate (d):
    return d.year * 10000 + d.month * 100 + d.day

# .................................................................................................

def time_roll (t, delta):
    assert isinstance (t, dt.time)
    assert isinstance (delta, dt.timedelta)
    
    return (dt.datetime.combine (_DUMMY_DATE, t) + delta).time ()

# .................................................................................................

def partition_range (r, n):
    assert len (r) == 2
    assert n >= len (r)
    
    ix = int (r [0])
    limit = int (r [1])
    assert ix < limit
    
    partitions = []
    
    ix_fp = float (ix)
    step = float (limit - ix) / n
    
    for i in xrange (n):
        ix_prev = ix
        ix = int (round (ix_fp + step * (i + 1)))
        partitions.append ((ix_prev, ix))
    
    assert (len (partitions) == n)
    
    partitions [-1] = (partitions [-1][0], limit) # guard against poor round-off 
    
    return partitions
    
# .................................................................................................

def int2alphabet (x):
    if x == 0:
        return 'A'
    tokens = []
    while x:
        tokens.append (chr (ord ('A') + x % 26))
        x /= 26
    return ''.join (tokens [::-1])   

# -------------------------------------------------------------------------------------------------
