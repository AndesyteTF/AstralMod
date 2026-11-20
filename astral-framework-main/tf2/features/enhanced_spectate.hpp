#pragma once

#include "link.hpp"

class enhanced_spectate
{
public:
    void toggle( u32 account );
    void apply( );
    void restore( );
    void clear( );

private:
    C_TFPlayer *find_target( ) const;

    u32     m_target_account{ };
    EHANDLE m_original_target{ };
    i32     m_original_mode{ OBS_MODE_NONE };
    bool    m_applied{ };
};

extern enhanced_spectate g_enhanced_spectate;
