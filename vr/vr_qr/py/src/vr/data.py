
from enum import Enum, unique
import re

import h5py
import numpy as np
import pandas as pd

# -------------------------------------------------------------------------------------------------

ATYPE_ATTR_NAME     = 'vr_at'
LABELS_ATTR_NAME    = 'vr_labels'

@unique
class atype (Enum):
    category, timestamp, i8, f8, i4, f4     = range (6)

ATYPE_DTYPE     = \
(
    np.int32,           # not used
    'datetime64[ns]',   # not used
    np.int64,
    np.float64,
    np.int32,
    np.float32
)
# .................................................................................................

_RE_MATCH_EVERYTHING    = re.compile (r'.*')
_RE_MATCH_NOTHING       = re.compile (r'^\b$')

# .................................................................................................

# TODO memoize category label arrays
# TODO read in chunks in parallel across all columns (and in the creation order)

def _create_col_category (ds, _, row_count):
    labels = ds.attrs [LABELS_ATTR_NAME][0]
    col_data = np.empty (row_count, dtype = np.int32)
    ds.read_direct (col_data) # note: read data in the creation pass
    return pd.Series (pd.Categorical.from_codes (col_data, categories = labels, ordered = False))

def _create_col_timestamp (ds, _, row_count):
    col_data = np.empty (row_count, dtype = np.int64) # 'h5py' doesn't support 'datetime64[ns]'
    ds.read_direct (col_data) # note: read data in the creation pass
    return col_data.view ('datetime64[ns]')


def _create_col_numeric (_, dtype, row_count):
    return np.empty (row_count, dtype = dtype)

# .................................................................................................

_NA_int64   = np.iinfo (np.int64).min
_NA_int32   = np.iinfo (np.int32).min
_NA_price   = float (_NA_int64) # prior to rescaling

def _read_col_category (ds, col_data, _):
    pass

def _read_col_timestamp (ds, col_data, _):
    pass

def _read_col_numeric (ds, col_data, is_price): # TODO handle NAs for other types 
    ds.read_direct (col_data)
    if is_price:
        col_data [col_data == _NA_price] = np.nan
        col_data /= 10000000.0

# .................................................................................................

_CREATE_COL     = \
(
    _create_col_category,
    _create_col_timestamp,
    _create_col_numeric,
    _create_col_numeric,
    _create_col_numeric,
    _create_col_numeric
)

_READ_COL_DATA   = \
(
    _read_col_category,
    _read_col_timestamp,
    _read_col_numeric,
    _read_col_numeric,
    _read_col_numeric,
    _read_col_numeric
)
# .................................................................................................

# TODO opt for how to load price values (fp or si)

def _is_included (s, include, exclude):
    return include.match (s) and not exclude.match (s)

def read_df (path, index = None, drop_index = True, prices = _RE_MATCH_NOTHING, include = _RE_MATCH_EVERYTHING, exclude = _RE_MATCH_NOTHING):
    if isinstance (prices, str): prices = re.compile (prices)
    if isinstance (include, str): include = re.compile (include)
    if isinstance (exclude, str): exclude = re.compile (exclude)
    
    data = { }
    
    with h5py.File (path, 'r') as f:
        row_count = None
        
        # allocate col memory:
        
        for name, ds in f.iteritems ():
            if name != index and not _is_included (name, include, exclude): continue
            
            row_count = len (ds) # note: the same for all columns
            
            at = ds.attrs [ATYPE_ATTR_NAME][0]
            if prices.match (name):
                at = atype.f8.value
            
            data [name] = _CREATE_COL [at](ds, ATYPE_DTYPE [at], row_count)
            
        df = pd.DataFrame (data, copy = False)
        
        # read actual col data:
        
        for name, ds in f.iteritems ():
            if name != index and not _is_included (name, include, exclude): continue

            at = ds.attrs [ATYPE_ATTR_NAME][0]
            _READ_COL_DATA [at](ds, df [name].values, prices.match (name))
        
        if index:
            df.set_index (index, drop = drop_index, inplace = True)
        
    return df

# .................................................................................................

def read_csv (path, timestamps = _RE_MATCH_NOTHING):
    if isinstance (timestamps, str): timestamps = re.compile (timestamps)
    header = pd.read_csv (path, nrows = 1).columns
    ts_indices = [c for c in xrange (header.shape [0]) if timestamps.match (header [c])]
    
    return pd.read_csv (path, parse_dates = ts_indices)

# -------------------------------------------------------------------------------------------------
