/*
 *  Created by Phil on 22/10/2010.
 *  Copyright 2010 Two Blue Cubes Ltd
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wpadded"
#endif

#include "catch.hpp"
#include "catch_text.h"

namespace Clara {

using namespace Catch;

class ArgData {
public:
    ArgData( std::string const& _arg ) : m_weight( 2 )
    {
        std::size_t first = _arg.find( '<' );
        std::size_t last = _arg.find_last_of( '>' );
        if( first == std::string::npos || last == std::string::npos || last <= first+1 )
            throw std::logic_error( "Argument must contain a name in angle brackets but it was: " + _arg );
        m_prefix = _arg.substr( 0, first );
        m_postfix = _arg.substr( last+1 );
        m_name = _arg.substr( first+1, last-first-1 );
        if( !m_prefix.empty() )
            --m_weight;
        if( !m_postfix.empty() )
            --m_weight;
    }
    std::string const& name() const { return m_name; }
    std::string const& prefix() const { return m_prefix; }
    std::string const& postfix() const { return m_postfix; }

    bool isMatch( std::string const& _arg ) const {
        return startsWith( _arg, m_prefix ) && endsWith( _arg, m_postfix );
    }
    std::string strip( std::string const& _arg ) const {
        return _arg.substr( m_prefix.size(),
                            _arg.size() - m_prefix.size() - m_postfix.size() );
    }
    bool operator < ( ArgData const& _other ) const {
        return m_weight < _other.m_weight;
    }
    
    friend std::ostream& operator << ( std::ostream& os, ArgData const& _arg ) {
        os << _arg.m_prefix << "<" << _arg.m_name << ">" << _arg.m_postfix;
        return os;
    }

protected:
    std::string m_prefix;
    std::string m_postfix;
    std::string m_name;
    int m_weight;
};

template<typename T>
bool convertInto( std::string const& _source, T& _dest ) {
    std::stringstream ss;
    ss << _source;
    ss >> _dest;
    return !ss.fail();
}
inline bool convertInto( std::string const& _source, std::string& _dest ) {
    _dest = _source;
    return true;
}

template<typename T>
class Opt {
public:
    Opt( std::string const& _synposis ) : m_synopsis( _synposis ) {}
    Opt& shortOpt( std::string const& _value ) { m_shortOpt = _value; return *this; }
    Opt& longOpt( std::string const& _value )  { m_longOpt = _value; return *this; }

    template<typename M>
    Opt& addArg( std::string const& _name, M const& _member  ){
        m_args.push_back( Arg( _name, _member ) );
        return *this;
    }
    
    std::size_t takesArg() const { return !m_args.empty(); }

    std::string synopsis() const { return m_synopsis; }
    std::string shortOpt() const { return m_shortOpt; }
    std::string longOpt() const { return m_longOpt; }
    
    bool parseInto( std::string const& _arg, T& _config ) const {
        ensureWeightedArgsAreSorted();
        typename std::vector<Arg const*>::const_iterator
            it = m_argsInWeightedOrder.begin(),
            itEnd = m_argsInWeightedOrder.end();
        for( ; it != itEnd; ++it )
            if( (*it)->isMatch( _arg ) ) {
                (*it)->parseInto( _arg, _config );
                return true;
            }
        return false;
    }

    std::string usage() const {
        std::ostringstream oss;
        oss << *this;
        return oss.str();
    }
    friend std::ostream& operator << ( std::ostream& os, Opt const& _opt ) {
        if( !_opt.m_shortOpt.empty() )
            os << "-" << _opt.m_shortOpt;
            if( !_opt.m_longOpt.empty() )
                os << ", ";
        if( !_opt.m_longOpt.empty() )
            os << "--" << _opt.m_longOpt;
        if( !_opt.m_args.empty() ) {
            os << " ";
            typename std::vector<Arg>::const_iterator
                it = _opt.m_args.begin(),
                itEnd = _opt.m_args.end();
            while( it != itEnd ) {
                os << static_cast<ArgData const&>( *it );
                if( ++it!=itEnd )
                    os << "|";
            }
        }
        return os;
    }
    
private:

    struct IField : SharedImpl<> {
        virtual ~IField() {}
        virtual bool parseInto( std::string const& _arg, T& _config ) const = 0;
    };
    
    template<typename F>
    struct Field;

    template<typename C, typename M>
    struct Field<M C::*> : IField {
        Field( M C::* _member ) : member( _member ) {}
        bool parseInto( std::string const& _arg, T& _config ) const {
            return convertInto( _arg, _config.*member );
        }
        M C::* member;
    };
    template<typename C, typename M>
    struct Field<void (C::*)( M )> : IField {
        Field( void (C::*_method)( M ) ) : method( _method ) {}
        bool parseInto( std::string const& _arg, T& _config ) const {
            M value;
            if( !convertInto( _arg, value ) )
                return false;
            ( _config.*method )( value );
            return true;
        }
        void (C::*method)( M );
    };
    
    class Arg : public ArgData {
    public:
        Arg() : m_field( NULL ) {}
        template<typename M>
        Arg( std::string const& _name, M const& _member )
        :   ArgData( _name ),
            m_field( new Field<M>( _member ) )
        {}
        void parseInto( std::string const& _arg, T& _config ) const {
            if( !m_field->parseInto( strip( _arg ), _config ) )
                throw std::domain_error( "'" + _arg + "' was not valid for <" + m_name + ">" );
        }
                
    private:
        Ptr<IField> m_field;
    };
    
    static bool argLess( Arg const* lhs, Arg const* rhs ) {
        return *lhs < *rhs;
    }
    void ensureWeightedArgsAreSorted() const {
        if( m_args.size() > m_argsInWeightedOrder.size() ) {
            m_argsInWeightedOrder.clear();
            typename std::vector<Arg>::const_iterator   it = m_args.begin(),
                                                        itEnd = m_args.end();
            for( ; it != itEnd; ++it )
                m_argsInWeightedOrder.push_back( &*it );
            sort( m_argsInWeightedOrder.begin(), m_argsInWeightedOrder.end(), &Opt::argLess );
        }
    }
    std::string m_synopsis;
    std::string m_shortOpt;
    std::string m_longOpt;
    std::vector<Arg> m_args;
    mutable std::vector<Arg const*> m_argsInWeightedOrder;
};

template<typename T>
class Parser
{
public:
    Parser()
    :   m_separatorChars( "=: " ),
        m_allowSpaceSeparator( m_separatorChars.find( ' ' ) != std::string::npos )
    {}

    Opt<T>& addOption( std::string const& _synposis ) {
        m_allOptionParsers.push_back( _synposis );
        return m_allOptionParsers.back();
    }

    void parseArgs( int argc, const char* const argv[], T& _config ) {
        std::vector<std::string> args;
        args.reserve( static_cast<std::size_t>( argc ) );
        for( int i = 0; i < argc; ++i )
            args.push_back( argv[i] );
        
        parseArgs( args, _config );
    }

    template<typename U>
    void parseRemainingArgs( Parser<U>& _parser, T& _config ) {
        parseArgs( _parser.m_unusedOpts, _config );
    }

    void parseArgs( std::vector<std::string> const& _args, T& _config ) {
        ensureOptions();
        for( std::size_t i = 0; i < _args.size(); ++i ) {
            std::string const& arg = _args[i];
            if( arg[0] == '-' ) {
                std::string optArgs, optName;
                std::size_t pos = arg.find_first_of( m_separatorChars );
                if( pos == std::string::npos ) {
                    optName = arg;
                }
                else {
                    optName = arg.substr(0, pos );
                    optArgs = arg.substr( pos+1 );
                }
                typename std::map<std::string, Opt<T> const*>::const_iterator it = m_optionsByName.find( optName );
                bool used = false;
                if( it != m_optionsByName.end() ) {
                    Opt<T> const& opt = *(it->second);                    
                    if( opt.takesArg() ) {
                        if( optArgs.empty() ) {
                            if( i < _args.size() && _args[i+1][0] != '-' )
                                optArgs = _args[++i];
                            else
                                throw std::domain_error( "Expected argument"); // !TBD better error
                        }
                    }
                    try {
                        used = opt.parseInto( optArgs, _config );
                    }
                    catch( std::exception& ex ) {
                        throw std::domain_error( "Error in " + optName + " option: " + ex.what() );
                    }
                }
                if( !used )
                    m_unusedOpts.push_back( arg );
            }
            else {
                m_args.push_back( arg );
            }
        }
    }
    
    friend std::ostream& operator <<( std::ostream& os, Parser const& _parser ) {
        typename std::vector<Opt<T> >::const_iterator it, itEnd = _parser.m_allOptionParsers.end();
        std::size_t maxWidth = 0;
        for(it = _parser.m_allOptionParsers.begin(); it != itEnd; ++it )
            maxWidth = (std::max)( it->usage().size(), maxWidth );
        
        for(it = _parser.m_allOptionParsers.begin(); it != itEnd; ++it ) {
            Text usage( it->usage(), TextAttributes().setWidth( maxWidth ) );
            // !TBD handle longer usage strings
            Text synopsis( it->synopsis(), TextAttributes().setWidth( CATCH_CONFIG_CONSOLE_WIDTH - maxWidth -3 ) );

            for( std::size_t i = 0; i < std::max( usage.size(), synopsis.size() ); ++i ) {
                std::string usageCol = i < usage.size() ? usage[i] : "";
                std::cout << usageCol;

                if( i < synopsis.size() && !synopsis[i].empty() )
                    std::cout   << std::string( 2 + maxWidth - usageCol.size(), ' ' )
                                << synopsis[i];
                std::cout << "\n";
            }            
        }
        return os;
    }
    
private:
    void ensureOptions() const {
        if( m_allOptionParsers.size() != m_optionsByName.size() ) {
            m_optionsByName.clear();
            typename std::vector<Opt<T> >::const_iterator it, itEnd = m_allOptionParsers.end();
            for( it = m_allOptionParsers.begin(); it != itEnd; ++it ) {
                if( !it->shortOpt().empty() )
                    m_optionsByName.insert( std::make_pair( "-" + it->shortOpt(), &*it ) );
                if( !it->longOpt().empty() )
                    m_optionsByName.insert( std::make_pair( "--" + it->longOpt(), &*it ) );                
            }
        }
    }
    template<typename U>
    friend class Parser;

    std::vector<Opt<T> > m_allOptionParsers;
    mutable std::map<std::string, Opt<T> const*> m_optionsByName;
    std::vector<std::string> m_args;
    std::vector<std::string> m_unusedOpts;
    std::string m_separatorChars;
    bool m_allowSpaceSeparator;
};

    
} // end namespace Catch

struct TestOpt {
    TestOpt() : number( 0 ), index( 0 ) {}

    std::string fileName;
    std::string streamName;
    int number;
    int index;
    
    void setValidIndex( int i ) {
        if( i < 0 || i > 10 )
            throw std::domain_error( "index must be between 0 and 10" );
        index = i;
    }
};

struct TestOpt2 {
    std::string description;
};

TEST_CASE( "Arg" ) {
    SECTION( "pre and post" ) {
        Clara::ArgData preAndPost( "prefix<arg>postfix" );
        CHECK( preAndPost.prefix() == "prefix" );
        CHECK( preAndPost.postfix() == "postfix" );
        CHECK( preAndPost.name() == "arg" );
        
        CHECK( preAndPost.isMatch( "prefixpayloadpostfix" ) );
        CHECK( preAndPost.strip( "prefixpayloadpostfix" ) == "payload" );
        CHECK_FALSE( preAndPost.isMatch( "payload" ) );
        CHECK_FALSE( preAndPost.isMatch( "postfixpayloadpostfix" ) );
        CHECK_FALSE( preAndPost.isMatch( "prefixpayloadpostfixx" ) );
    }
    SECTION( "pre" ) {
        Clara::ArgData preAndPost( "prefix<arg>" );
        CHECK( preAndPost.prefix() == "prefix" );
        CHECK( preAndPost.postfix() == "" );
        CHECK( preAndPost.name() == "arg" );
        
        CHECK( preAndPost.isMatch( "prefixpayload" ) );
        CHECK( preAndPost.strip( "prefixpayload" ) == "payload" );
        CHECK_FALSE( preAndPost.isMatch( "payload" ) );
        CHECK_FALSE( preAndPost.isMatch( "postfixpayload" ) );
    }
    SECTION( "post" ) {
        Clara::ArgData preAndPost( "<arg>postfix" );
        CHECK( preAndPost.prefix() == "" );
        CHECK( preAndPost.postfix() == "postfix" );
        CHECK( preAndPost.name() == "arg" );

        CHECK( preAndPost.isMatch( "payloadpostfix" ) );
        CHECK( preAndPost.strip( "payloadpostfix" ) == "payload" );
        CHECK_FALSE( preAndPost.isMatch( "payload" ) );
        CHECK_FALSE( preAndPost.isMatch( "payloadpostfixx" ) );
    }
    SECTION( "none" ) {
        Clara::ArgData preAndPost( "<arg>" );
        CHECK( preAndPost.prefix() == "" );
        CHECK( preAndPost.postfix() == "" );
        CHECK( preAndPost.name() == "arg" );

        CHECK( preAndPost.isMatch( "payload" ) );
        CHECK( preAndPost.strip( "payload" ) == "payload" );
    }
    SECTION( "errors" ) {
        CHECK_THROWS( Clara::ArgData( "" ) );
        CHECK_THROWS( Clara::ArgData( "no brackets" ) );
        CHECK_THROWS( Clara::ArgData( "<one bracket" ) );
        CHECK_THROWS( Clara::ArgData( "one bracket>" ) );
        CHECK_THROWS( Clara::ArgData( "><" ) );
        CHECK_THROWS( Clara::ArgData( "<>" ) );
    }
}

TEST_CASE( "cmdline", "" ) {

    TestOpt config;
    Clara::Parser<TestOpt> parser;
    parser.addOption( "specifies output file" )
        .shortOpt( "o" )
        .longOpt( "output" )
        .addArg( "<filename>", &TestOpt::fileName )
        .addArg( "%<stream name>", &TestOpt::streamName );

    SECTION( "plain filename" ) {
        const char* argv[] = { "test", "-o filename.ext" };

        parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config );
        CHECK( config.fileName == "filename.ext" );
        CHECK( config.streamName == "" );
    }
    SECTION( "plain filename with colon" ) {
        const char* argv[] = { "test", "-o:filename.ext" };

        parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config );
        CHECK( config.fileName == "filename.ext" );
        CHECK( config.streamName == "" );
    }
    SECTION( "plain filename with =" ) {
        const char* argv[] = { "test", "-o=filename.ext" };

        parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config );
        CHECK( config.fileName == "filename.ext" );
        CHECK( config.streamName == "" );
    }
    SECTION( "stream name" ) {
        const char* argv[] = { "test", "-o %stdout" };

        parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config );
        CHECK( config.fileName == "" );
        CHECK( config.streamName == "stdout" );
    }
    SECTION( "long opt" ) {
        const char* argv[] = { "test", "--output %stdout" };

        parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config );
        CHECK( config.fileName == "" );
        CHECK( config.streamName == "stdout" );
    }
    
    parser.addOption( "a number" )
            .shortOpt( "n" )
            .addArg( "<an integral value>", &TestOpt::number );
    
    SECTION( "a number" ) {
        const char* argv[] = { "test", "-n 42" };

        parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config );
        CHECK( config.number == 42 );
    }
    SECTION( "not a number" ) {
        const char* argv[] = { "test", "-n forty-two" };

        CHECK_THROWS( parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config ) );
        CHECK( config.number == 0 );
    }
    
    SECTION( "two parsers" ) {

        TestOpt config1;
        TestOpt2 config2;
        Clara::Parser<TestOpt2> parser2;

        parser2.addOption( "description" )
                    .shortOpt( "d" )
                    .longOpt( "description" )
                    .addArg( "<some text>", &TestOpt2::description );
        
        const char* argv[] = { "test", "-n 42", "-d some text" };

        parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config1 );
        CHECK( config1.number == 42 );
        
        parser2.parseRemainingArgs( parser, config2 );
        CHECK( config2.description == "some text" );
        
    }

    SECTION( "methods" ) {
        parser.addOption( "An index, which is an integer between 0 and 10, inclusive" )
                .shortOpt( "i" )
                .addArg( "<index>", &TestOpt::setValidIndex );

        SECTION( "in range" ) {
            const char* argv[] = { "test", "-i 3" };

            parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config );
            REQUIRE( config.index == 3 );
        }
        SECTION( "out of range" ) {
            const char* argv[] = { "test", "-i 42" };

            REQUIRE_THROWS( parser.parseArgs( sizeof(argv)/sizeof(char*), argv, config ) );
        }
        
        std::cout << parser << std::endl;
    }
    
}
