//
// created by jrmadsen on Thu Nov 16 00:55:30 2017
//

#ifndef auto_timer_hpp_
#define auto_timer_hpp_

#include "timing_manager.hpp"
#include <string>
#include <cstdint>

namespace toast
{
namespace util
{

class auto_timer
{
public:
    typedef toast::util::timing_manager::toast_timer_t  toast_timer_t;

public:
    // Constructor and Destructors
    auto_timer(const std::string&, const int32_t& lineno,
               const std::string& = "cxx", bool temp_disable = false);
    virtual ~auto_timer();

private:
    static uint64_t& ncount()
    { return details::base_timer::get_instance_count(); }

    static uint64_t& nhash()
    { return details::base_timer::get_instance_hash(); }

private:
    bool            m_temp_disable;
    uint64_t        m_hash;
    toast_timer_t*    m_timer;
    toast_timer_t     m_temp_timer;
};

//----------------------------------------------------------------------------//
inline auto_timer::auto_timer(const std::string& timer_tag,
                              const int32_t& lineno,
                              const std::string& code_tag,
                              bool temp_disable)
: m_temp_disable(temp_disable),
  m_hash(lineno),
  m_timer(nullptr)
{
    // for consistency, always increment hash keys
    ++auto_timer::ncount();
    auto_timer::nhash() += m_hash;

    if(timing_manager::is_enabled() &&
       (uint64_t) timing_manager::max_depth() > auto_timer::ncount())
    {
        m_timer = &timing_manager::instance()->timer(timer_tag, code_tag,
                                                     auto_timer::ncount(),
                                                     auto_timer::nhash());

        m_temp_timer.start();
    }

    if(m_temp_disable && timing_manager::instance()->is_enabled())
        timing_manager::instance()->enable(false);
}
//----------------------------------------------------------------------------//
inline auto_timer::~auto_timer()
{
    if(m_temp_disable && ! timing_manager::instance()->is_enabled())
        timing_manager::instance()->enable(true);

    // for consistency, always decrement hash keys
    if(auto_timer::ncount() > 0)
        --auto_timer::ncount();
    auto_timer::nhash() -= m_hash;

    if(m_timer)
    {
        m_temp_timer.stop();
        *m_timer += m_temp_timer;
    }
}
//----------------------------------------------------------------------------//

} // namespace util

} // namespace toast

//----------------------------------------------------------------------------//

typedef toast::util::auto_timer                     auto_timer_t;
#if defined(DISABLE_TIMERS)
#   define TOAST_AUTO_TIMER(str)
#else
#   define AUTO_TIMER_NAME_COMBINE(X, Y) X##Y
#   define AUTO_TIMER_NAME(Y) AUTO_TIMER_NAME_COMBINE(macro_auto_timer, Y)
#   define TOAST_AUTO_TIMER(str) \
        auto_timer_t AUTO_TIMER_NAME(__LINE__)(std::string(__FUNCTION__) + std::string(str), __LINE__)
#endif

//----------------------------------------------------------------------------//

#endif

