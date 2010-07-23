#ifndef TRACELIB_H
#define TRACELIB_H

#include "tracelib_config.h"

#include "backtrace.h"

#include <ctime>
#include <vector>

#ifdef _MSC_VER
#  define TRACELIB_BEACON(verbosity) \
{ \
    static TRACELIB_NAMESPACE_IDENT(TracePoint) tracePoint(TRACELIB_NAMESPACE_IDENT(TracePoint)::LogPoint, (verbosity), __FILE__, __LINE__, __FUNCSIG__); \
    TRACELIB_NAMESPACE_IDENT(getActiveTrace()->visitTracePoint( &tracePoint )); \
}

#  define TRACELIB_SNAPSHOT_MSG(verbosity, vars, msg) \
{ \
    static TRACELIB_NAMESPACE_IDENT(TracePoint) tracePoint(TRACELIB_NAMESPACE_IDENT(TracePoint)::WatchPoint, (verbosity), __FILE__, __LINE__, __FUNCSIG__); \
    std::vector<TRACELIB_NAMESPACE_IDENT(AbstractVariableConverter) *> *variableSnapshot = new std::vector<TRACELIB_NAMESPACE_IDENT(AbstractVariableConverter) *>; \
    (*variableSnapshot) << vars; \
    TRACELIB_NAMESPACE_IDENT(getActiveTrace()->visitTracePoint( &tracePoint, (msg), variableSnapshot )); \
}
#  define TRACELIB_SNAPSHOT(verbosity, vars) TRACELIB_SNAPSHOT_MSG(verbosity, vars, 0)

#  define TRACELIB_VISIT_TRACEPOINT_MSG(type, verbosity, msg) \
{ \
    static TRACELIB_NAMESPACE_IDENT(TracePoint) tracePoint(type, (verbosity), __FILE__, __LINE__, __FUNCSIG__); \
    TRACELIB_NAMESPACE_IDENT(getActiveTrace()->visitTracePoint( &tracePoint, msg )); \
}

#  define TRACELIB_VAR(v) TRACELIB_NAMESPACE_IDENT(makeConverter(#v, v))
#else
#  error "Unsupported compiler!"
#endif

#define TRACELIB_DEBUG TRACELIB_DEBUG_MSG(0)
#define TRACELIB_ERROR TRACELIB_ERROR_MSG(0)
#define TRACELIB_TRACE TRACELIB_TRACE_MSG(0)
#define TRACELIB_WATCH(vars) TRACELIB_WATCH_MSG(0, msg)
#define TRACELIB_DEBUG_MSG(msg) TRACELIB_VISIT_TRACEPOINT_MSG(TRACELIB_NAMESPACE_IDENT(TracePoint)::DebugPoint, 1, msg)
#define TRACELIB_ERROR_MSG(msg) TRACELIB_VISIT_TRACEPOINT_MSG(TRACELIB_NAMESPACE_IDENT(TracePoint)::ErrorPoint, 1, msg)
#define TRACELIB_TRACE_MSG(msg) TRACELIB_VISIT_TRACEPOINT_MSG(TRACELIB_NAMESPACE_IDENT(TracePoint)::LogPoint, 1, msg)
#define TRACELIB_WATCH_MSG(msg, vars) TRACELIB_SNAPSHOT_MSG(1, vars, msg)

TRACELIB_NAMESPACE_BEGIN

template <typename T>
std::string convertVariable( T o );

class AbstractVariableConverter
{
public:
    virtual const char *name() const = 0;
    virtual std::string toString() const = 0;
};

template <typename T>
class VariableConverter : public AbstractVariableConverter
{
public:
    VariableConverter( const char *name, const T &o ) : m_name( name ), m_o( o ) { }

    const char *name() const { return m_name; }

    virtual std::string toString() const {
        return convertVariable( m_o );
    }

private:
    const char *m_name;
    const T &m_o;
};

template <typename T>
AbstractVariableConverter *makeConverter(const char *name, const T &o) {
    return new VariableConverter<T>( name, o );
}

class Output
{
public:
    virtual ~Output();

    virtual bool canWrite() const { return true; }
    virtual void write( const std::vector<char> &data ) = 0;

protected:
    Output();

private:
    Output( const Output &rhs );
    void operator=( const Output &other );
};

struct TracePoint;

struct TraceEntry {
    TraceEntry( const TracePoint *tracePoint_, const char *msg = 0)
        : timeStamp( std::time( NULL ) ),
        tracePoint( tracePoint_ ),
        backtrace( 0 ),
        variables( 0 ),
        message( msg )
    {
    }

    ~TraceEntry() {
        if ( variables ) {
            std::vector<AbstractVariableConverter *>::const_iterator it, end = variables->end();
            for ( it = variables->begin(); it != end; ++it ) {
                delete *it;
            }
        }
        delete variables;
        delete backtrace;
    }

    const time_t timeStamp;
    const TracePoint *tracePoint;
    std::vector<AbstractVariableConverter *> *variables;
    Backtrace *backtrace;
    const char * const message;
};

std::vector<AbstractVariableConverter *> &operator<<( std::vector<AbstractVariableConverter *> &v,
                                                      AbstractVariableConverter *c );

class Serializer
{
public:
    virtual ~Serializer();

    virtual std::vector<char> serialize( const TraceEntry &entry ) = 0;

protected:
    Serializer();

private:
    Serializer( const Serializer &rhs );
    void operator=( const Serializer &other );
};

class Filter
{
public:
    virtual ~Filter();

    virtual bool acceptsTracePoint( const TracePoint *tracePoint ) = 0;

protected:
    Filter();

private:
    Filter( const Filter &rhs );
    void operator=( const Filter &other );
};

class Configuration;

struct TracePoint {
    enum Type {
        ErrorPoint,
        DebugPoint,
        LogPoint,
        WatchPoint
    };

    TracePoint( Type type_, unsigned short verbosity_, const char *sourceFile_, unsigned int lineno_, const char *functionName_ )
        : type( type_ ),
        verbosity( verbosity_ ),
        sourceFile( sourceFile_ ),
        lineno( lineno_ ),
        functionName( functionName_ ),
        lastUsedConfiguration( 0 ),
        active( false ),
        backtracesEnabled( false ),
        variableSnapshotEnabled( false )
    {
    }

    const Type type;
    const unsigned short verbosity;
    const char * const sourceFile;
    const unsigned int lineno;
    const char * const functionName;
    const Configuration *lastUsedConfiguration;
    bool active;
    bool backtracesEnabled;
    bool variableSnapshotEnabled;
};

class TracePointSet
{
public:
    static const unsigned int IgnoreTracePoint = 0x0000;
    static const unsigned int LogTracePoint = 0x0001;
    static const unsigned int YieldBacktrace = LogTracePoint | 0x0100;
    static const unsigned int YieldVariables = LogTracePoint | 0x0200;

    TracePointSet( Filter *filter, unsigned int actions );
    ~TracePointSet();

    unsigned int considerTracePoint( const TracePoint *tracePoint );

private:
    TracePointSet( const TracePointSet &other );
    void operator=( const TracePointSet &rhs );

    Filter *m_filter;
    const unsigned int m_actions;
};

class Trace
{
public:
    Trace();
    ~Trace();

    void reconsiderTracePoint( TracePoint *tracePoint ) const;
    void visitTracePoint( TracePoint *tracePoint,
                          const char *msg = 0,
                          std::vector<AbstractVariableConverter *> *variables = 0 );

    void setSerializer( Serializer *serializer );
    void setOutput( Output *output );
    void addTracePointSet( TracePointSet *tracePointSet );

private:
    Trace( const Trace &trace );
    void operator=( const Trace &trace );

    Serializer *m_serializer;
    Output *m_output;
    std::vector<TracePointSet *> m_tracePointSets;
    Configuration *m_configuration;
    BacktraceGenerator m_backtraceGenerator;
};

Trace *getActiveTrace();
void setActiveTrace( Trace *trace );

TRACELIB_NAMESPACE_END

#endif // !defined(TRACELIB_H)
