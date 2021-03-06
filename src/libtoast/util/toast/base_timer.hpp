/*
Copyright (c) 2015-2018 by the parties listed in the AUTHORS file.
All rights reserved.  Use of this source code is governed by
a BSD-style license that can be found in the LICENSE file.
*/

#ifndef base_timer_hpp_
#define base_timer_hpp_

//----------------------------------------------------------------------------//

#include "base_clock.hpp"
#include "rss.hpp"
#include <fstream>
#include <string>
#include <atomic>

#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include <cereal/macros.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/vector.hpp>

//----------------------------------------------------------------------------//

namespace toast
{

enum class timer_field
{
    wall,
    user,
    system,
    cpu,
    percent,
    total_curr,
    total_peak,
    self_curr,
    self_peak

};

namespace util
{
namespace details
{

//============================================================================//
//
//  Class for handling the RSS (resident set size) memory usage
//
//============================================================================//

class base_rss_usage
{
public:
    typedef toast::rss::usage    rss_usage_t;

    inline void init()
    {
        m_rss_tmp.record();
    }

    inline void record()
    {
        m_rss_self.record(m_rss_tmp);
        m_rss_tot.record();
    }

    rss_usage_t& total() { return m_rss_tot; }
    rss_usage_t& self()  { return m_rss_self; }

    const rss_usage_t& total() const { return m_rss_tot; }
    const rss_usage_t& self() const { return m_rss_self; }

    void max(const base_rss_usage& rhs)
    {
        m_rss_tot  = toast::rss::usage::max(m_rss_tot, rhs.total());
        m_rss_self = toast::rss::usage::max(m_rss_self, rhs.self());
    }

protected:
    // memory usage
    rss_usage_t             m_rss_tot;
    rss_usage_t             m_rss_self;
    rss_usage_t             m_rss_tmp;

};

//============================================================================//
//
//  Class for handling the timing and memory data
//
//============================================================================//

class base_timer_data
{
public:
    typedef base_timer_data                             this_type;
    typedef std::micro                                  ratio_t;
    typedef toast::util::base_clock<ratio_t>         clock_t;
    typedef clock_t::time_point                         time_point_t;
    typedef std::tuple<time_point_t, time_point_t>      data_type;
    typedef base_rss_usage                              rss_type;
    typedef std::chrono::duration<clock_t, ratio_t>     duration_t;

public:
    base_timer_data() : m_running(false), m_data(data_type()) { }

    time_point_t& start() { m_running = true; return std::get<0>(m_data); }
    time_point_t& stop() { m_running = false; return std::get<1>(m_data); }

    const time_point_t& start() const { m_running = true; return std::get<0>(m_data); }
    const time_point_t& stop() const { m_running = false; return std::get<1>(m_data); }

    const bool& running() const { return m_running; }

    template <typename Archive> void
    serialize(Archive& ar, const unsigned int /*version*/)
    {
        ar(cereal::make_nvp("start", std::get<0>(m_data)),
           cereal::make_nvp("stop",  std::get<1>(m_data)));
    }

    inline void rss_init() { m_rss.init(); }
    inline void rss_record() { m_rss.record(); }

    inline rss_type& rss() { return m_rss; }
    inline const rss_type& rss() const { return m_rss; }

protected:
    mutable bool    m_running;
    data_type       m_data;
    rss_type        m_rss;

};

//----------------------------------------------------------------------------//

template <int N>
uint64_t get_start(const base_timer_data& data)
{
    return std::get<N>(data.start().time_since_epoch().count().data);
}

//----------------------------------------------------------------------------//

template <int N>
uint64_t get_stop(const base_timer_data& data)
{
    return std::get<N>(data.stop().time_since_epoch().count().data);
}

//============================================================================//
//
//  Class for handling the timing difference
//
//============================================================================//

class base_timer_delta
{
public:
    typedef base_timer_delta                            this_type;
    typedef uint64_t                                    uint_type;
    typedef std::tuple<uint_type, uint_type, uint_type> data_type;
    typedef std::tuple<uint64_t, uint64_t, uint64_t>    incr_type;
    typedef base_timer_data                             op_type;
    typedef base_rss_usage                              rss_type;

public:
    base_timer_delta()
    : m_lap(0),
      m_sum(data_type(0, 0, 0)),
      m_sqr(data_type(0, 0, 0)),
      m_rss(rss_type())
    { }

    void reset()
    {
        m_lap = 0;
        m_sum = data_type(0, 0, 0);
        m_sqr = data_type(0, 0, 0);
        m_rss = rss_type();
    }

    this_type& operator+=(const op_type& data)
    {
        auto _data = incr_type(compute<0>(data),
                               compute<1>(data),
                               compute<2>(data));
        compute_sum(_data);
        //compute_sqr(_data);
        m_lap += 1;
        m_rss.max(data.rss());

        return *this;
    }

    this_type& operator+=(const this_type& rhs)
    {
        m_lap += rhs.m_lap;
        compute_sum(rhs.m_sum);
        //compute_sqr(rhs.m_sqr);
        m_rss.max(rhs.m_rss);
        return *this;
    }

    template <int N> uint64_t get_sum() const { return std::get<N>(m_sum); }
    template <int N> uint64_t get_sqr() const { return std::get<N>(m_sqr); }
    uint64_t size() const { return m_lap; }

    inline rss_type& rss() { return m_rss; }
    inline const rss_type& rss() const { return m_rss; }

protected:
    template <int N> uint64_t compute(const op_type& data)
    {
        auto _ts = get_start<N>(data);
        auto _te = get_stop<N>(data);
        return (_te > _ts) ? (_te - _ts) : uint64_t(0);
    }

    inline
    void compute_sum(const incr_type& rhs)
    {
        std::get<0>(m_sum) += std::get<0>(rhs);
        std::get<1>(m_sum) += std::get<1>(rhs);
        std::get<2>(m_sum) += std::get<2>(rhs);
    }

    inline
    void compute_sqr(const incr_type& rhs)
    {
        std::get<0>(m_sum) += std::pow(std::get<0>(rhs), 2);
        std::get<1>(m_sum) += std::pow(std::get<1>(rhs), 2);
        std::get<2>(m_sum) += std::pow(std::get<2>(rhs), 2);
    }

protected:
    uint_type m_lap;
    data_type m_sum;
    data_type m_sqr;
    rss_type  m_rss;

};

//============================================================================//
//
//  Primary base class for handling the timer
//
//============================================================================//


class base_timer
{
public:
    template <typename _Key, typename _Mapped>
    using uomap = std::unordered_map<_Key, _Mapped>;

    typedef std::string                         string_t;
    typedef string_t::size_type                 size_type;
    typedef std::mutex                          mutex_t;
    typedef std::recursive_mutex                rmutex_t;
    typedef std::ostream                        ostream_t;
    typedef std::ofstream                       ofstream_t;
    typedef uomap<ostream_t*, rmutex_t>         mutex_map_t;
    typedef std::lock_guard<mutex_t>            auto_lock_t;
    typedef std::lock_guard<rmutex_t>           recursive_lock_t;
    typedef tms                                 tms_t;
    typedef base_timer_data                     data_t;
    typedef data_t::ratio_t                     ratio_t;
    typedef data_t::clock_t                     base_clock_t;
    typedef data_t::time_point_t                time_point_t;
    typedef base_timer_delta                    data_accum_t;
    typedef data_t::duration_t                  duration_t;
    typedef base_timer                          this_type;
    //typedef uomap<const base_timer*, data_t>    data_map_t;
    //typedef std::unique_ptr<data_map_t>         data_map_ptr_t;
    typedef base_rss_usage                      rss_usage_t;

public:
    base_timer(uint16_t = 3, const string_t& =
               "%w wall, %u user + %s system = %t CPU [sec] (%p%)"
               " : total rss %c | %m  : self rss %C | %M [MB]\n",
               ostream_t* = &std::cout);
    virtual ~base_timer();

    base_timer(const base_timer& rhs);
    base_timer& operator=(const base_timer& rhs);

public:
    // static public functions
    static uint64_t& get_instance_count() { return f_instance_count; }
    static uint64_t& get_instance_hash()  { return f_instance_hash; }

public:
    inline void start();
    inline void stop();
    inline bool is_valid() const;
    double real_elapsed() const;
    double system_elapsed() const;
    double user_elapsed() const;
    double cpu_elapsed() const { return user_elapsed() + system_elapsed(); }
    double cpu_utilization() const { return cpu_elapsed() / real_elapsed() * 100.; }
    inline const char* clock_time() const;
    inline size_type laps() const { return m_accum.size(); }
    inline void rss_init();
    inline void rss_record();
    inline void reset() { m_accum.reset(); }

public:
    void report(ostream_t&, bool endline = true, bool no_min = false) const;
    inline void report(bool endline = true) const;
    inline void report_average(bool endline = true) const;
    inline void report_average(ostream_t& os, bool endline = true) const;

protected:
    typedef std::pair<size_type, timer_field>   fieldpos_t;
    typedef std::pair<string_t,  timer_field>   fieldstr_t;
    typedef std::vector<fieldpos_t>             poslist_t;
    typedef std::vector<fieldstr_t>             strlist_t;

protected:
    void parse_format();
    virtual void compose() = 0;
    data_t& m_timer() const;

    data_accum_t& get_accum() { return m_accum; }
    const data_accum_t& get_accum() const { return m_accum; }

protected:
    // PODs
    uint16_t                m_precision;
    // pointers
    ostream_t*              m_os;
    // lists
    poslist_t               m_format_positions;
    mutable data_accum_t    m_accum;
    // strings
    string_t                m_format_string;
    // data
    mutable data_t          m_data;
    // mutex
    mutex_t                 m_mutex;

private:
    // hash and data fields
    static thread_local uint64_t        f_instance_count;
    static thread_local uint64_t        f_instance_hash;
    //static thread_local data_map_ptr_t  f_data_map;
    // world mutex map, thread-safe ostreams
    static mutex_map_t                  w_mutex_map;

public:
    template <typename Archive> void
    serialize(Archive& ar, const unsigned int /*version*/)
    {
        ar(cereal::make_nvp("laps", m_accum.size()),
           // user clock elapsed
           cereal::make_nvp("user_elapsed",     m_accum.get_sum<0>()),
           //cereal::make_nvp("user_elapsed_sqr", m_accum.get_sqr<0>()),
           // system clock elapsed
           cereal::make_nvp("system_elapsed",      m_accum.get_sum<1>()),
           //cereal::make_nvp("system_elapsed_sqr",  m_accum.get_sqr<1>()),
           // wall clock elapsed
           cereal::make_nvp("wall_elapsed",     m_accum.get_sum<2>()),
           //cereal::make_nvp("wall_elapsed_sqr", m_accum.get_sqr<2>()),
           // cpu elapsed
           cereal::make_nvp("cpu_elapsed",
                            m_accum.get_sum<0>() + m_accum.get_sum<1>()),
           // cpu utilization
           cereal::make_nvp("cpu_util",
                            (m_accum.get_sum<0>() + m_accum.get_sum<1>())
                            / m_accum.get_sum<2>()),
           // conversion to seconds
           cereal::make_nvp("to_seconds_ratio_num", ratio_t::num),
           cereal::make_nvp("to_seconds_ratio_den", ratio_t::den),
           // memory usage
           cereal::make_nvp("rss_max",  m_accum.rss().total()),
           cereal::make_nvp("rss_self", m_accum.rss().self()));
    }

};

//----------------------------------------------------------------------------//
inline void base_timer::rss_init()
{
    m_timer().rss_init();
}
//----------------------------------------------------------------------------//
inline void base_timer::rss_record()
{
    m_timer().rss_record();
}
//----------------------------------------------------------------------------//
// Print timer status n std::ostream
static inline
std::ostream& operator<<(std::ostream& os, const base_timer& t)
{
    bool restart = !t.is_valid();
    if(restart)
        const_cast<base_timer&>(t).stop();
    t.report(os);
    if(restart)
        const_cast<base_timer&>(t).start();

    return os;
}
//----------------------------------------------------------------------------//
inline                                                          // Wall time
double base_timer::real_elapsed() const
{
    if(m_timer().running())
        throw std::runtime_error("Error! base_timer::real_elapsed() - "
                                 "timer not stopped or no times recorded!");
    return m_accum.get_sum<2>() / static_cast<double>(ratio_t::den);
}
//----------------------------------------------------------------------------//
inline                                                          // System time
double base_timer::system_elapsed() const
{
    if(m_timer().running())
        throw std::runtime_error("Error! base_timer::system_elapsed() - "
                                 "timer not stopped or no times recorded!");
    return m_accum.get_sum<1>() / static_cast<double>(ratio_t::den);
}
//----------------------------------------------------------------------------//
inline                                                          // CPU time
double base_timer::user_elapsed() const
{
    if(m_timer().running())
        throw std::runtime_error("Error! base_timer::user_elapsed() - "
                                 "timer not stopped or no times recorded!");
    return m_accum.get_sum<0>() / static_cast<double>(ratio_t::den);
}
//----------------------------------------------------------------------------//
inline
void base_timer::start()
{
    if(!m_timer().running())
    {
        m_timer().start() = base_clock_t::now();
        rss_init();
    }
}
//----------------------------------------------------------------------------//
inline
void base_timer::stop()
{
    if(m_timer().running())
    {
        m_timer().stop() = base_clock_t::now();
        rss_record();
        //auto_lock_t l(f_mutex_map[this]);
        m_accum += m_timer();
    }
}
//----------------------------------------------------------------------------//
inline
bool base_timer::is_valid() const
{
    return (m_timer().running()) ? false : true;
}
//----------------------------------------------------------------------------//
inline const char* base_timer::clock_time() const
{
    time_t rawtime;
    struct tm* timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    return asctime (timeinfo);
}
//----------------------------------------------------------------------------//
inline
void base_timer::report(bool endline) const
{
    this->report(*m_os, endline);
}
//----------------------------------------------------------------------------//
inline
void base_timer::report_average(bool endline) const
{
    this->report(*m_os, endline, true);
}
//----------------------------------------------------------------------------//
inline
void base_timer::report_average(ostream_t& os, bool endline) const
{
    this->report(os, endline, true);
}
//----------------------------------------------------------------------------//
inline
base_timer::data_t& base_timer::m_timer() const
{
    return m_data;
    //if(!f_data_map)
    //    f_data_map.reset(new data_map_t());
    //if(f_data_map->find(this) == f_data_map->end())
    //    f_data_map->insert(std::make_pair(this, data_t()));
    //return f_data_map->find(this)->second;
}
//----------------------------------------------------------------------------//

} // namespace details

} // namespace util

} // namespace toast

//----------------------------------------------------------------------------//

namespace internal
{
typedef typename toast::util::details::base_timer_data::ratio_t base_ratio_t;
typedef toast::util::base_clock<base_ratio_t>   base_clock_t;
typedef toast::util::base_clock_data<base_ratio_t> base_clock_data_t;
typedef std::chrono::duration<base_clock_data_t, base_ratio_t> base_duration_t;
typedef std::chrono::time_point<base_clock_t, base_duration_t>  base_time_point_t;
typedef std::tuple<base_time_point_t, base_time_point_t> base_time_pair_t;
}

//----------------------------------------------------------------------------//

#endif // base_timer_hpp_
