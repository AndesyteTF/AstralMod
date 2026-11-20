#include "link.hpp"

class fakelag
{
public:
    void think( );
    void enforce_retain_blastjump( );
    bool is_active( );

    void reset( );

private:
    bool should_run( );
    void retain_blastjump( );

    // The initial goal choke.
    i32 m_goal_choke{ 0 };

    // The absolute goal choke after needed additions.
    i32 m_abs_goal_choke{ 0 };

    bool m_is_active{ false };
    bool m_preserving_blastjump{ false };
    bool m_was_grounded{ true };

    config::fakelag_profile_t m_profile;
};

extern fakelag g_fakelag;
