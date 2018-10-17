/* tracetool - a framework for tracing the execution of C++ programs
 * Copyright 2010-2016 froglogic GmbH
 *
 * This file is part of tracetool.
 *
 * tracetool is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tracetool is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with tracetool.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "configuration.h"
#include "log.h"
#include "filter.h"
#include "output.h"
#include "serializer.h"
#include "trace.h"

#include "3rdparty/tinyxml/tinyxml.h"

#include <fstream>

#include <string.h>

using namespace std;

static bool fileExists( const string &filename )
{
   return ifstream( filename.c_str() ).is_open();
}

static std::string getText( const TiXmlElement *e )
{
    const char *s = e->GetText();
    return s ? s
             : "";
}

TRACELIB_NAMESPACE_BEGIN

Configuration *Configuration::fromFile( const string &fileName, Log *log )
{
    Configuration *cfg = new Configuration( log );
    if ( !cfg->loadFromFile( fileName ) ) {
        delete cfg;
        return 0;
    }
    return cfg;
}

Configuration *Configuration::fromMarkup( const string &markup, Log *log )
{
    Configuration *cfg = new Configuration( log );
    if ( !cfg->loadFromMarkup( markup ) ) {
        delete cfg;
        return 0;
    }
    return cfg;
}

Configuration::Configuration( Log *log )
    : m_fileName( "<null>"),
    m_configuredSerializer( 0 ),
    m_configuredOutput( 0 ),
    m_log( log )
{
}

bool Configuration::loadFromFile( const string &fileName )
{
    m_fileName = fileName;;

    if ( !fileExists( m_fileName ) ) {
        return false;
    }

    TiXmlDocument xmlDoc;
    if ( !xmlDoc.LoadFile( m_fileName.c_str() ) ) {
        m_log->writeError( "Tracelib Configuration: Failed to load XML file from %s", m_fileName.c_str() );
        return false;
    }

    return loadFrom( &xmlDoc );
}

bool Configuration::loadFromMarkup( const string &markup )
{
    TiXmlDocument xmlDoc;
    xmlDoc.Parse( markup.c_str() ); // XXX Error handling
    return loadFrom( &xmlDoc );
}

bool Configuration::loadFrom( TiXmlDocument *xmlDoc )
{
    TiXmlElement *rootElement = xmlDoc->RootElement();
    if ( rootElement->ValueStr() != "tracelibConfiguration" ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: unexpected root element '%s' found", m_fileName.c_str(), rootElement->Value() );
        return false;
    }

    static string myProcessName = currentProcessName();

    for ( TiXmlElement *e = rootElement->FirstChildElement(); e; e = e->NextSiblingElement() ) {
        if ( e->ValueStr() == "process" ) {
            TiXmlElement *nameElement = e->FirstChildElement( "name" );
            if ( !nameElement ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: found <process> element without <name> child element.", m_fileName.c_str() );
                return false;
            }

            string processBaseName = getText( nameElement );
            string platformProcessName = executableName( processBaseName );

            // XXX Consider encoding issues (e.g. if myProcessName contains umlauts)
#ifdef _WIN32
            const bool isMyProcessElement = _stricmp( myProcessName.c_str(),
                                                      platformProcessName.c_str() ) == 0;
#else
            const bool isMyProcessElement = platformProcessName == myProcessName;
#endif
            if ( isMyProcessElement ) {
                m_log->writeStatus( "Tracelib Configuration: found configuration for process %s (matches executable: %s)", processBaseName.c_str(), myProcessName.c_str() );
                return readProcessElement( e );
            }
            continue;
        }

        if ( e->ValueStr() == "tracekeys" ) {
            if ( !readTraceKeysElement( e ) ) {
                return false;
            }
            continue;
        }

        if ( e->ValueStr() == "storage" ) {
            if ( !readStorageElement( e ) ) {
                return false;
            }
            continue;
        }

        m_log->writeError( "Tracelib Configuration: while reading %s: unexpected child element '%s' found inside <tracelibConfiguration>.", m_fileName.c_str(), e->Value() );
        return false;
    }
    m_log->writeStatus( "Tracelib Configuration: no configuration found for process %s", myProcessName.c_str() );
    return true;
}

bool Configuration::readProcessElement( TiXmlElement *processElement )
{
    for ( TiXmlElement *e = processElement->FirstChildElement(); e; e = e->NextSiblingElement() ) {
        if ( e->ValueStr() == "name" ) {
            continue;
        }

        if ( e->ValueStr() == "serializer" ) {
            if ( m_configuredSerializer ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: found multiple <serializer> elements in <process> element.", m_fileName.c_str() );
                return false;
            }

            Serializer *s = createSerializerFromElement( e );
            if ( !s ) {
                return false;
            }

            m_configuredSerializer = s;
            continue;
        }

        if ( e->ValueStr() == "tracepointset" ) {
            TracePointSet *tracePointSet = createTracePointSetFromElement( e );
            if ( !tracePointSet ) {
                return false;
            }
            m_configuredTracePointSets.push_back( tracePointSet );
            continue;
        }

        if ( e->ValueStr() == "output" ) {
            if ( m_configuredOutput ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: found multiple <output> elements in <process> element.", m_fileName.c_str() );
                return false;
            }
            Output *output = createOutputFromElement( e );
            if ( !output ) {
                return false;
            }
            m_configuredOutput = output;
            continue;
        }

        m_log->writeError( "Tracelib Configuration: while reading %s: unexpected child element '%s' found inside <process>.", m_fileName.c_str(), processElement->Value() );
    }
    return true;
}

bool Configuration::readTraceKeysElement( TiXmlElement *traceKeysElem )
{
    for ( TiXmlElement *e = traceKeysElem->FirstChildElement(); e; e = e->NextSiblingElement() ) {
        if ( e->ValueStr() == "key" ) {
            // XXX consider encoding issues
            TraceKey k;
            k.name = getText( e );
            std::string enabledValue = "true";
            if ( e->QueryValueAttribute( "enabled", &enabledValue ) == TIXML_SUCCESS ) {
                k.enabled = enabledValue == "true";
            }
            m_configuredTraceKeys.push_back( k );
            continue;
        }

        m_log->writeError( "Tracelib Configuration: while reading %s: unexpected child element '%s' found inside <tracekeys>.", m_fileName.c_str(), e->Value() );
        return false;
    }
    return true;
}

const StorageConfiguration &Configuration::storageConfiguration() const
{
    return m_storageConfiguration;
}

const vector<TracePointSet *> &Configuration::configuredTracePointSets() const
{
    return m_configuredTracePointSets;
}

Serializer *Configuration::configuredSerializer()
{
    return m_configuredSerializer;
}

Output *Configuration::configuredOutput()
{
    return m_configuredOutput;
}

const vector<TraceKey> &Configuration::configuredTraceKeys() const
{
    return m_configuredTraceKeys;
}

Filter *Configuration::createFilterFromElement( TiXmlElement *e )
{
    if ( e->ValueStr() == "matchanyfilter" ) {
        DisjunctionFilter *f = new DisjunctionFilter;
        for ( TiXmlElement *childElement = e->FirstChildElement(); childElement; childElement = childElement->NextSiblingElement() ) {
            Filter *subFilter = createFilterFromElement( childElement );
            if ( !subFilter ) {
                // XXX Yield diagnostics;
                delete f;
                return 0;
            }
            f->addFilter( subFilter );
        }
        return f;
    }

    if ( e->ValueStr() == "matchallfilter" ) {
        ConjunctionFilter *f = new ConjunctionFilter;
        for ( TiXmlElement *childElement = e->FirstChildElement(); childElement; childElement = childElement->NextSiblingElement() ) {
            Filter *subFilter = createFilterFromElement( childElement );
            if ( !subFilter ) {
                // XXX Yield diagnostics;
                delete f;
                return 0;
            }
            f->addFilter( subFilter );
        }
        return f;
    }

    if ( e->ValueStr() == "pathfilter" ) {
        MatchingMode matchingMode;
        string matchingModeValue = "strict";
        if ( e->QueryValueAttribute( "matchingmode", &matchingModeValue ) == TIXML_SUCCESS ) {
            if ( matchingModeValue == "strict" ) {
                matchingMode = StrictMatch;
            } else if ( matchingModeValue == "regexp" ) {
                matchingMode = RegExpMatch;
            } else if ( matchingModeValue == "wildcard" ) {
                matchingMode = WildcardMatch;
            } else {
                m_log->writeError( "Tracelib Configuration: while reading %s: unsupported matching mode '%s' specified for <pathfilter> element.", m_fileName.c_str(), matchingModeValue.c_str() );
                return 0;
            }
        }
        const char *pathFilterValue = e->GetText();
        if ( !pathFilterValue ) return 0;
        PathFilter *f = new PathFilter;
        f->setPath( matchingMode, pathFilterValue ); // XXX Consider encoding issues
        return f;
    }

    if ( e->ValueStr() == "functionfilter" ) {
        MatchingMode matchingMode;
        string matchingModeValue = "strict";
        if ( e->QueryValueAttribute( "matchingmode", &matchingModeValue ) == TIXML_SUCCESS ) {
            if ( matchingModeValue == "strict" ) {
                matchingMode = StrictMatch;
            } else if ( matchingModeValue == "regexp" ) {
                matchingMode = RegExpMatch;
            } else if ( matchingModeValue == "wildcard" ) {
                matchingMode = WildcardMatch;
            } else {
                m_log->writeError( "Tracelib Configuration: while reading %s: unsupported matching mode '%s' specified for <functionfilter> element.", m_fileName.c_str(), matchingModeValue.c_str() );
                return 0;
            }
        }
        const char *functionFilterValue = e->GetText();
        if ( !functionFilterValue ) return 0;
        FunctionFilter *f = new FunctionFilter;
        f->setFunction( matchingMode, functionFilterValue ); // XXX Consider encoding issues
        return f;
    }

    if ( e->ValueStr() == "tracekeyfilter" ) {
        GroupFilter::Mode mode = GroupFilter::Whitelist;

        string modeValue;
        if ( e->QueryValueAttribute( "mode", &modeValue ) == TIXML_SUCCESS ) {
            if ( modeValue == "whitelist" ) {
                mode = GroupFilter::Whitelist;
            } else if ( modeValue == "blacklist" ) {
                mode = GroupFilter::Blacklist;
            } else {
                m_log->writeError( "Tracelib Configuration: while reading %s: unsupported mode '%s' specified for <tracekeyfilter> element.", m_fileName.c_str(), modeValue.c_str() );
                return 0;
            }
        }

        GroupFilter *f = new GroupFilter;
        f->setMode( mode );
        for ( TiXmlElement *childElement = e->FirstChildElement(); childElement; childElement = childElement->NextSiblingElement() ) {
            if ( childElement->ValueStr() == "key" ) {
                f->addGroupName( getText( childElement ) ); // XXX Consider encoding issues
            } else {
                delete f;
                m_log->writeError( "Tracelib Configuration: while reading %s: unsupported child element '%s' specified for <tracekeyfilter> element.", m_fileName.c_str(), childElement->ValueStr().c_str() );
                return 0;
            }
        }

        return f;
    }

    m_log->writeError( "Tracelib Configuration: while reading %s: Unexpected filter element '%s' found.", m_fileName.c_str(), e->Value() );
    return 0;
}

Serializer *Configuration::createSerializerFromElement( TiXmlElement *e )
{
    string serializerType;
    if ( e->QueryValueAttribute( "type", &serializerType ) != TIXML_SUCCESS ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: Failed to read type property of <serializer> element.", m_fileName.c_str() );
        return 0;
    }

    if ( serializerType == "plaintext" ) {
        PlaintextSerializer *serializer = new PlaintextSerializer;
        for ( TiXmlElement *optionElement = e->FirstChildElement(); optionElement; optionElement = optionElement->NextSiblingElement() ) {
            if ( optionElement->ValueStr() != "option" ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unexpected element '%s' in <serializer> element of type plaintext found.", m_fileName.c_str(), optionElement->Value() );
                delete serializer;
                return 0;
            }

            string optionName;
            if ( optionElement->QueryValueAttribute( "name", &optionName ) != TIXML_SUCCESS ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Failed to read name property of <option> element; ignoring this.", m_fileName.c_str() );
                continue;
            }

            if ( optionName == "timestamps" ) {
                serializer->setTimestampsShown( getText( optionElement ) == "yes" );
            } else {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unknown <option> element with name '%s' found in plaintext serializer; ignoring this.", m_fileName.c_str(), optionName.c_str() );
                continue;
            }
        }
        m_log->writeStatus( "Tracelib Configuration: using plaintext serializer" );
        return serializer;
    }

    if ( serializerType == "xml" ) {
        bool beautifiedOutput = false;
        for ( TiXmlElement *optionElement = e->FirstChildElement(); optionElement; optionElement = optionElement->NextSiblingElement() ) {
            if ( optionElement->ValueStr() != "option" ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unexpected element '%s' in <serializer> element of type xml found.", m_fileName.c_str(), optionElement->Value() );
                return 0;
            }

            string optionName;
            if ( optionElement->QueryValueAttribute( "name", &optionName ) != TIXML_SUCCESS ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Failed to read name property of <serializer> element; ignoring this.", m_fileName.c_str() );
                continue;
            }

            if ( optionName == "beautifiedOutput" ) {
                beautifiedOutput = getText( optionElement ) == "yes";
            } else {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unknown <option> element with name '%s' found in xml serializer; ignoring this.", m_fileName.c_str(), optionName.c_str() );
                continue;
            }
        }
        XMLSerializer *serializer = new XMLSerializer;
        serializer->setBeautifiedOutput( beautifiedOutput );
        m_log->writeStatus( "Tracelib Configuration: using XML serializer (beautified output=%d)", beautifiedOutput );
        return serializer;
    }

    m_log->writeError( "Tracelib Configuration: while reading %s: <serializer> element with unknown type '%s' found.", m_fileName.c_str(), serializerType.c_str() );
    return 0;
}

TracePointSet *Configuration::createTracePointSetFromElement( TiXmlElement *e )
{
    string backtracesAttr = "no";
    e->QueryValueAttribute( "backtraces", &backtracesAttr );
    if ( backtracesAttr != "yes" && backtracesAttr != "no" ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: Invalid value '%s' for backtraces= attribute of <tracepointset> element", m_fileName.c_str(), backtracesAttr.c_str() );
        return 0;
    }

    string variablesAttr = "no";
    e->QueryValueAttribute( "variables", &variablesAttr );
    if ( variablesAttr != "yes" && variablesAttr != "no" ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: Invalid value '%s' for variables= attribute of <tracepointset> element", m_fileName.c_str(), variablesAttr.c_str() );
        return 0;
    }

    TiXmlElement *filterElement = e->FirstChildElement();
    if ( !filterElement ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: No filter element specified for <tracepointset> element", m_fileName.c_str() );
        return 0;
    }

    ConjunctionFilter *filter = new ConjunctionFilter;
    while ( filterElement ) {
        Filter *subFilter = createFilterFromElement( filterElement );
        if ( !subFilter ) {
            delete filter;
            return 0;
        }
        filter->addFilter( subFilter );
        filterElement = filterElement->NextSiblingElement();
    }

    int actions = TracePointSet::LogTracePoint;
    if ( backtracesAttr == "yes" ) {
        actions |= TracePointSet::YieldBacktrace;
    }
    if ( variablesAttr == "yes" ) {
        actions |= TracePointSet::YieldVariables;
    }

    return new TracePointSet( filter, actions );
}

Output *Configuration::createOutputFromElement( TiXmlElement *e )
{
    string outputType;
    if ( e->QueryValueAttribute( "type", &outputType ) == TIXML_NO_ATTRIBUTE ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: No type= attribute specified for <output> element", m_fileName.c_str() );
        return 0;
    }

    if ( outputType == "stdout" ) {
        m_log->writeStatus( "Tracelib Configuration: using stdout output" );
        return new StdoutOutput;
    }

    if ( outputType == "file" ) {
        std::string filename;
        bool overwriteExistingFile = true;
        bool relativePathIsRelativeToUserHome = false;
        for ( TiXmlElement *optionElement = e->FirstChildElement(); optionElement; optionElement = optionElement->NextSiblingElement() ) {
            if ( optionElement->ValueStr() != "option" ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unexpected element '%s' in <output> element of type file found.", m_fileName.c_str(), optionElement->Value() );
                return 0;
            }

            string optionName;
            if ( optionElement->QueryValueAttribute( "name", &optionName ) != TIXML_SUCCESS ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Failed to read name property of <option> element; ignoring this.", m_fileName.c_str() );
                continue;
            }

            if ( optionName == "filename" ) {
                filename = getText( optionElement ); // XXX Consider encoding issues
            } else if ( optionName == "overwriteExistingFile" ) {
                overwriteExistingFile = getText( optionElement ) == "true";
            } else if ( optionName == "relativeToUserHome" ) {
                relativePathIsRelativeToUserHome = getText( optionElement ) == "true";
            } else {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unknown <option> element with name '%s' found in file output; ignoring this.", m_fileName.c_str(), optionName.c_str() );
                continue;
            }
        }

        if ( filename.empty() ) {
            m_log->writeError( "Tracelib Configuration: while reading %s: No 'filename' option specified for <output> element of type filename.", m_fileName.c_str() );
            return 0;
        }
        if( !isAbsolute( filename ) && relativePathIsRelativeToUserHome ) {
            filename = userHome() + pathSeparator() + filename;
        }
        if( !overwriteExistingFile ) {
            string basename = filename.substr(0, filename.rfind("."));
            string ext = filename.substr(filename.rfind(".")+1);
            int cnt = 1;
            while( fileExists( filename ) )
            {
                std::stringstream sstr;
                sstr << basename << "_" << cnt << "." << ext;
                cnt += 1;
                filename = sstr.str();
            }
        }
        m_log->writeStatus( "Tracelib Configuration: using file output to %s", filename.c_str() );
        return new FileOutput( m_log, filename );
    }

    if ( outputType == "tcp" ) {
        string hostname;
        unsigned short port = TRACELIB_DEFAULT_PORT;
        for ( TiXmlElement *optionElement = e->FirstChildElement(); optionElement; optionElement = optionElement->NextSiblingElement() ) {
            if ( optionElement->ValueStr() != "option" ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unexpected element '%s' in <output> element of type tcp found.", m_fileName.c_str(), optionElement->Value() );
                return 0;
            }

            string optionName;
            if ( optionElement->QueryValueAttribute( "name", &optionName ) != TIXML_SUCCESS ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: Failed to read name property of <option> element; ignoring this.", m_fileName.c_str() );
                continue;
            }

            if ( optionName == "host" ) {
                hostname = getText( optionElement ); // XXX Consider encoding issues
            } else if ( optionName == "port" ) {
                istringstream str( getText( optionElement ) );
                str >> port; // XXX Error handling for non-numeric port numbers
            } else {
                m_log->writeError( "Tracelib Configuration: while reading %s: Unknown <option> element with name '%s' found in tcp output; ignoring this.", m_fileName.c_str(), optionName.c_str() );
                continue;
            }
        }

        if ( hostname.empty() ) {
            m_log->writeError( "Tracelib Configuration: while reading %s: No 'host' option specified for <output> element of type tcp.", m_fileName.c_str() );
            return 0;
        }

        if ( port == 0 ) {
            m_log->writeError( "Tracelib Configuration: while reading %s: No 'port' option specified for <output> element of type tcp.", m_fileName.c_str() );
            return 0;
        }

        m_log->writeStatus( "Tracelib Configuration: using TCP/IP output, remote = %s:%d", hostname.c_str(), port );
        return new NetworkOutput( m_log, hostname.c_str(), port );
    }

    m_log->writeError( "Tracelib Configuration: while reading %s: Unknown type '%s' specified for <output> element", m_fileName.c_str(), outputType.c_str() );
    return 0;
}

bool Configuration::readStorageElement( TiXmlElement *storageElem )
{
    bool haveMaximumSize = false;
    bool haveShrinkBy = false;
    bool haveArchiveDirectory = false;
    for ( TiXmlElement *e = storageElem->FirstChildElement(); e; e = e->NextSiblingElement() ) {
        if ( e->ValueStr() == "maximumSize" ) {
            if ( haveMaximumSize ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: duplicated <maximumSize> specified in <storage>", m_fileName.c_str() );
                return false;
            }

            const std::string txt = getText( e );
            if ( txt.empty() ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: empty <maximumSize> specified in <storage>", m_fileName.c_str() );
                return false;
            }

            istringstream str( txt );
            str >> m_storageConfiguration.maximumTraceSize; // XXX Error handling for non-numeric values
            haveMaximumSize = true;
            continue;
        }

        if ( e->ValueStr() == "shrinkBy" ) {
            if ( haveShrinkBy ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: duplicate <shrinkBy> specified in <storage>", m_fileName.c_str() );
                return false;
            }

            const std::string txt = getText( e );
            if ( txt.empty() ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: empty <shrinkBy> specified in <storage>", m_fileName.c_str() );
                return false;
            }

            istringstream str( txt );
            str >> m_storageConfiguration.shrinkPercentage; // XXX Error handling for non-numeric values
            haveShrinkBy = true;
            continue;
        }

        if ( e->ValueStr() == "archiveDirectory" ) {
            if ( haveArchiveDirectory ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: duplicate <archiveDirectory> specified in <storage>", m_fileName.c_str() );
                return false;
            }

            const std::string txt = getText( e );
            if ( txt.empty() ) {
                m_log->writeError( "Tracelib Configuration: while reading %s: empty <archiveDirectory> specified in <storage>", m_fileName.c_str() );
                return false;
            }
            m_storageConfiguration.archiveDirectoryName = txt;
            haveArchiveDirectory = true;
            continue;
        }

        m_log->writeError( "Tracelib Configuration: while reading %s: unexpected element <%s> specified in <storage>", e->ValueStr().c_str(), m_fileName.c_str() );
        return false;
    }

    if ( !haveMaximumSize ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: <maximumSize> element missing in <storage>", m_fileName.c_str() );
        return false;
    }

    if ( !haveShrinkBy ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: <shrinkBy> element missing in <storage>", m_fileName.c_str() );
        return false;
    }

    if ( !haveArchiveDirectory ) {
        m_log->writeError( "Tracelib Configuration: while reading %s: <archiveDirectory> element missing in <storage>", m_fileName.c_str() );
        return false;
    }

    return true;
}

TRACELIB_NAMESPACE_END

