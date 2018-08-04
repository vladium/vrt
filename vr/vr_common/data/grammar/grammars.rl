//----------------------------------------------------------------------------
%%{

#.............................................................................

    machine common;
    
        WS              = space*;

        num_int         = [+\-]? digit+;
        num_fp          = [+\-]? digit* '.'? digit+ ([eE] [+\-]? digit+)?;
        
        year            = [12]digit{3};
        month           = ( 'Jan' | 'Feb' | 'Mar' | 'Apr' | 'May' | 'Jun' | 'Jul' | 'Aug' | 'Sep' | 'Oct' | 'Nov' | 'Dec' );
        day             = [0-3]digit; 
        
        hh              = [0-2]digit;
        mm              = [0-5]digit;
        ss              = mm;
        
        datetime        = year'-'month'-'day ' ' hh':'mm':'ss ( '.' digit{1,9} )?;
    
#.............................................................................
    
    machine CSV;
    include common;
    
        nc              = [.0-9a-zA-Z_];
        
        NA_token        = 'NA';
        qname           = ( '\'' nc+ '\'' ) | ( '"' nc+ '"' );
        
#.............................................................................
    
    machine JSON;
    include common;

        null_token      = 'null';
        boolean         = ( 'true' | 'false' );      
    
        esc             = '\\';
        escaped_c       = esc (["/bfnrt] | esc | ('u' [0-9a-fA-F]{4}));
        
        string          = '"' ((any - ( '"' | esc )) | escaped_c)* '"';
        
#.............................................................................

    machine attributes;
    include common;

        wc              = [0-9a-zA-Z_];
        nc              = [.0-9a-zA-Z_];
      
        word            = wc+;
        name            = nc+;
      
        ts              = ( 'ts' | 'time'('stamp')? );
        i4              = 'i4';
        i8              = 'i8';
        f4              = 'f4';
        f8              = 'f8';
      
}%%
//----------------------------------------------------------------------------
