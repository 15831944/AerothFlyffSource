/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2001-2007 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(TRACE_MACRO_EXPANSION_HPP_D8469318_8407_4B9D_A19F_13CA60C1661F_INCLUDED)
#define TRACE_MACRO_EXPANSION_HPP_D8469318_8407_4B9D_A19F_13CA60C1661F_INCLUDED

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <ostream>
#include <string>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>

#include <boost/wave/token_ids.hpp>
#include <boost/wave/util/macro_helpers.hpp>
#include <boost/wave/preprocessing_hooks.hpp>
#include <boost/wave/whitespace_handling.hpp>
#include <boost/wave/language_support.hpp>

#include "stop_watch.hpp"

#ifdef BOOST_NO_STRINGSTREAM
#include <strstream>
#define BOOST_WAVE_OSSTREAM std::ostrstream
std::string BOOST_WAVE_GETSTRING(std::ostrstream& ss)
{
    ss << std::ends;
    std::string rval = ss.str();
    ss.freeze(false);
    return rval;
}
#else
#include <sstream>
#define BOOST_WAVE_GETSTRING(ss) ss.str()
#define BOOST_WAVE_OSSTREAM std::ostringstream
#endif

//  trace_flags:  enable single tracing functionality
enum trace_flags {
    trace_nothing = 0,      // disable tracing
    trace_macros = 1,       // enable macro tracing
    trace_includes = 2      // enable include file tracing
};

///////////////////////////////////////////////////////////////////////////////
//
//  Special error thrown whenever the #pragma wave system() directive is 
//  disabled
//
///////////////////////////////////////////////////////////////////////////////
class no_pragma_system_exception :
    public boost::wave::preprocess_exception
{
public:
    enum error_code {
        pragma_system_not_enabled = boost::wave::preprocess_exception::last_error_number + 1
    };
    
    no_pragma_system_exception(char const *what_, error_code code, int line_, 
        int column_, char const *filename_) throw() 
    :   boost::wave::preprocess_exception(what_, 
            (boost::wave::preprocess_exception::error_code)code, line_, 
            column_, filename_)
    {
    }
    ~no_pragma_system_exception() throw() {}
    
    
    virtual char const *what() const throw()
    {
        return "boost::wave::no_pragma_system_exception";
    }
    virtual bool is_recoverable() const throw()
    {
        return true;
    }
    virtual int get_severity() const throw()
    {
        return boost::wave::util::severity_remark;
    }
    
    static char const *error_text(int code)
    {
        return "the directive '#pragma wave system()' was not enabled, use the "
               "-x command line argument to enable the execution of";
    }
    static boost::wave::util::severity severity_level(int code)
    {
        return boost::wave::util::severity_remark;
    }
    static char const *severity_text(int code)
    {
        return boost::wave::util::get_severity(boost::wave::util::severity_remark);
    }
};

///////////////////////////////////////////////////////////////////////////////
//  
//  The trace_macro_expansion policy is used to trace the macro expansion of
//  macros whenever it is requested from inside the input stream to preprocess
//  through the '#pragma wave_option(trace: enable)' directive. The macro 
//  tracing is disabled with the help of a '#pragma wave_option(trace: disable)'
//  directive.
//
//  This policy type is used as a template parameter to the boost::wave::context<>
//  object.
//
///////////////////////////////////////////////////////////////////////////////
template <typename TokenT>
class trace_macro_expansion
:   public boost::wave::context_policies::eat_whitespace<TokenT>
{
    typedef boost::wave::context_policies::eat_whitespace<TokenT> base_type;
    
public:
    trace_macro_expansion(bool preserve_whitespace_, 
            std::ofstream &output_, std::ostream &tracestrm_, 
            std::ostream &includestrm_, trace_flags flags_, 
            bool enable_system_command_, bool& generate_output_,
            std::string const& default_outfile_)
    :   outputstrm(output_), tracestrm(tracestrm_), includestrm(includestrm_), 
        level(0), flags(flags_), logging_flags(trace_nothing), 
        enable_system_command(enable_system_command_),
        preserve_whitespace(preserve_whitespace_),
        generate_output(generate_output_),
        default_outfile(default_outfile_)
    {
        using namespace std;    // some systems have time in namespace std
        time(&started_at);
    }
    ~trace_macro_expansion()
    {
    }
    
    ///////////////////////////////////////////////////////////////////////////
    //  
    //  The function 'expanding_function_like_macro' is called, whenever a 
    //  function-like macro is to be expanded.
    //
    //  The 'macrodef' parameter marks the position, where the macro to expand 
    //  is defined.
    //  The 'formal_args' parameter holds the formal arguments used during the
    //  definition of the macro.
    //  The 'definition' parameter holds the macro definition for the macro to 
    //  trace.
    //
    //  The 'macrocall' parameter marks the position, where this macro invoked.
    //  The 'arguments' parameter holds the macro arguments used during the 
    //  invocation of the macro
    //
    ///////////////////////////////////////////////////////////////////////////
    template <typename ContainerT>
    void expanding_function_like_macro(
        TokenT const &macrodef, std::vector<TokenT> const &formal_args, 
        ContainerT const &definition,
        TokenT const &macrocall, std::vector<ContainerT> const &arguments) 
    {
        if (!enabled_macro_tracing()) return;
        
        if (0 == get_level()) {
        // output header line
        BOOST_WAVE_OSSTREAM stream;

            stream 
                << macrocall.get_position() << ": "
                << macrocall.get_value() << "(";

        // argument list
            for (typename ContainerT::size_type i = 0; i < arguments.size(); ++i) {
                stream << boost::wave::util::impl::as_string(arguments[i]);
                if (i < arguments.size()-1)
                    stream << ", ";
            }
            stream << ")" << std::endl; 
            output(BOOST_WAVE_GETSTRING(stream));
            increment_level();
        }        
        
    // output definition reference
        {
        BOOST_WAVE_OSSTREAM stream;

            stream 
                << macrodef.get_position() << ": see macro definition: "
                << macrodef.get_value() << "(";

        // formal argument list
            for (typename std::vector<TokenT>::size_type i = 0; 
                i < formal_args.size(); ++i) 
            {
                stream << formal_args[i].get_value();
                if (i < formal_args.size()-1)
                    stream << ", ";
            }
            stream << ")" << std::endl; 
            output(BOOST_WAVE_GETSTRING(stream));
        }

        if (formal_args.size() > 0) {
        // map formal and real arguments
            open_trace_body("invoked with\n");
            for (typename std::vector<TokenT>::size_type j = 0; 
                j < formal_args.size(); ++j) 
            {
                using namespace boost::wave;

                BOOST_WAVE_OSSTREAM stream;
                stream << formal_args[j].get_value() << " = ";
#if BOOST_WAVE_SUPPORT_VARIADICS_PLACEMARKERS != 0
                if (T_ELLIPSIS == token_id(formal_args[j])) {
                // ellipsis
                    for (typename ContainerT::size_type k = j; 
                        k < arguments.size(); ++k) 
                    {
                        stream << boost::wave::util::impl::as_string(arguments[k]);
                        if (k < arguments.size()-1)
                            stream << ", ";
                    }
                } 
                else 
#endif
                {
                    stream << boost::wave::util::impl::as_string(arguments[j]);
                }
                stream << std::endl;
                output(BOOST_WAVE_GETSTRING(stream));
            }
            close_trace_body();
        }
        open_trace_body();
    }

    ///////////////////////////////////////////////////////////////////////////
    //  
    //  The function 'expanding_object_like_macro' is called, whenever a 
    //  object-like macro is to be expanded .
    //
    //  The 'macrodef' parameter marks the position, where the macro to expand 
    //  is defined.
    //  The 'definition' parameter holds the macro definition for the macro to 
    //  trace.
    //
    //  The 'macrocall' parameter marks the position, where this macro invoked.
    //
    ///////////////////////////////////////////////////////////////////////////
    template <typename ContainerT>
    void expanding_object_like_macro(TokenT const &macrodef, 
        ContainerT const &definition, TokenT const &macrocall)
    {
        if (!enabled_macro_tracing()) return;
        
        if (0 == get_level()) {
        // output header line
        BOOST_WAVE_OSSTREAM stream;

            stream 
                << macrocall.get_position() << ": "
                << macrocall.get_value() << std::endl;
            output(BOOST_WAVE_GETSTRING(stream));
            increment_level();
        }
        
    // output definition reference
        {
        BOOST_WAVE_OSSTREAM stream;

            stream 
                << macrodef.get_position() << ": see macro definition: "
                << macrodef.get_value() << std::endl;
            output(BOOST_WAVE_GETSTRING(stream));
        }
        open_trace_body();
    }
    
    ///////////////////////////////////////////////////////////////////////////
    //  
    //  The function 'expanded_macro' is called, whenever the expansion of a 
    //  macro is finished but before the rescanning process starts.
    //
    //  The parameter 'result' contains the token sequence generated as the 
    //  result of the macro expansion.
    //
    ///////////////////////////////////////////////////////////////////////////
    template <typename ContainerT>
    void expanded_macro(ContainerT const &result)
    {
        if (!enabled_macro_tracing()) return;
        
        BOOST_WAVE_OSSTREAM stream;
        stream << boost::wave::util::impl::as_string(result) << std::endl;
        output(BOOST_WAVE_GETSTRING(stream));

        open_trace_body("rescanning\n");
    }

    ///////////////////////////////////////////////////////////////////////////
    //  
    //  The function 'rescanned_macro' is called, whenever the rescanning of a 
    //  macro is finished.
    //
    //  The parameter 'result' contains the token sequence generated as the 
    //  result of the rescanning.
    //
    ///////////////////////////////////////////////////////////////////////////
    template <typename ContainerT>
    void rescanned_macro(ContainerT const &result)
    {
        if (!enabled_macro_tracing() || get_level() == 0) 
            return;

        BOOST_WAVE_OSSTREAM stream;
        stream << boost::wave::util::impl::as_string(result) << std::endl;
        output(BOOST_WAVE_GETSTRING(stream));
        close_trace_body();
        close_trace_body();
        
        if (1 == get_level())
            decrement_level();
    }

    ///////////////////////////////////////////////////////////////////////////
    //  
    //  The function 'interpret_pragma' is called, whenever a #pragma wave 
    //  directive is found, which isn't known to the core Wave library. 
    //
    //  The parameter 'ctx' is a reference to the context object used for 
    //  instantiating the preprocessing iterators by the user.
    //
    //  The parameter 'pending' may be used to push tokens back into the input 
    //  stream, which are to be used as the replacement text for the whole 
    //  #pragma wave() directive.
    //
    //  The parameter 'option' contains the name of the interpreted pragma.
    //
    //  The parameter 'values' holds the values of the parameter provided to 
    //  the pragma operator.
    //
    //  The parameter 'act_token' contains the actual #pragma token, which may 
    //  be used for error output.
    //
    //  If the return value is 'false', the whole #pragma directive is 
    //  interpreted as unknown and a corresponding error message is issued. A
    //  return value of 'true' signs a successful interpretation of the given 
    //  #pragma.
    //
    ///////////////////////////////////////////////////////////////////////////
    template <typename ContextT, typename ContainerT>
    bool 
    interpret_pragma(ContextT &ctx, ContainerT &pending, 
        typename ContextT::token_type const &option, ContainerT const &valuetokens, 
        typename ContextT::token_type const &act_token)
    {
        typedef typename ContextT::token_type token_type;

        ContainerT values(valuetokens);
        boost::wave::util::impl::trim_sequence(values);    // trim whitespace

        if (option.get_value() == "timer") {
        // #pragma wave timer(value)
            if (0 == values.size()) {
            // no value means '1'
                using namespace boost::wave;
                timer(token_type(T_INTLIT, "1", act_token.get_position()));
            }
            else {
                timer(values.front());
            }
            return true;
        }
        if (option.get_value() == "trace") {
        // enable/disable tracing option
            return interpret_pragma_trace(ctx, values, act_token);
        }
        if (option.get_value() == "system") {
            if (!enable_system_command) {
            // if the #pragma wave system() directive is not enabled, throw
            // a corresponding error (actually its a remark),
                BOOST_WAVE_THROW(no_pragma_system_exception, 
                    pragma_system_not_enabled,
                    boost::wave::util::impl::as_string(values).c_str(), 
                    act_token.get_position());
            }
            
        // try to spawn the given argument as a system command and return the
        // std::cout of this process as the replacement of this _Pragma
            return interpret_pragma_system(ctx, pending, values, act_token);
        }
        if (option.get_value() == "stop") {
        // stop the execution and output the argument
            BOOST_WAVE_THROW(preprocess_exception, error_directive,
                boost::wave::util::impl::as_string(values).c_str(), 
                act_token.get_position());
        }
        if (option.get_value() == "option") {
        // handle different options 
            return interpret_pragma_option(ctx, values, act_token);
        }
        return false;
    }
        
    ///////////////////////////////////////////////////////////////////////////
    //  
    //  The function 'opened_include_file' is called, whenever a file referred 
    //  by an #include directive was successfully located and opened.
    //
    //  The parameter 'filename' contains the file system path of the 
    //  opened file (this is relative to the directory of the currently 
    //  processed file or a absolute path depending on the paths given as the
    //  include search paths).
    //
    //  The include_depth parameter contains the current include file depth.
    //
    //  The is_system_include parameter denotes, whether the given file was 
    //  found as a result of a #include <...> directive.
    //  
    ///////////////////////////////////////////////////////////////////////////
    void 
    opened_include_file(std::string const &relname, std::string const &absname, 
        std::size_t include_depth, bool is_system_include) 
    {
        if (enabled_include_tracing()) {
            // print indented filename
            for (std::size_t i = 0; i < include_depth; ++i)
                includestrm << " ";
                
            if (is_system_include)
                includestrm << "<" << relname << "> (" << absname << ")";
            else
                includestrm << "\"" << relname << "\" (" << absname << ")";

            includestrm << std::endl;
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    //
    //  The function 'may_skip_whitespace' is called, will be called by the 
    //  library, whenever a token is about to be returned to the calling 
    //  application. 
    //
    //  The parameter 'ctx' is a reference to the context object used for 
    //  instantiating the preprocessing iterators by the user.
    //
    //  The 'token' parameter holds a reference to the current token. The policy 
    //  is free to change this token if needed.
    //
    //  The 'skipped_newline' parameter holds a reference to a boolean value 
    //  which should be set to true by the policy function whenever a newline 
    //  is going to be skipped. 
    //
    //  If the return value is true, the given token is skipped and the 
    //  preprocessing continues to the next token. If the return value is 
    //  false, the given token is returned to the calling application. 
    //
    //  ATTENTION!
    //  Caution has to be used, because by returning true the policy function 
    //  is able to force skipping even significant tokens, not only whitespace. 
    //
    ///////////////////////////////////////////////////////////////////////////
    template <typename ContextT>
    bool may_skip_whitespace(ContextT const &ctx, TokenT &token, bool &skipped_newline)
    {
        return this->base_type::may_skip_whitespace(ctx, token, skipped_newline) ?
            !preserve_whitespace : false;
    }

protected:
    ///////////////////////////////////////////////////////////////////////////
    //  Interpret the different Wave specific pragma directives/operators
    template <typename ContextT, typename ContainerT>
    bool 
    interpret_pragma_trace(ContextT const &/*ctx*/, ContainerT const &values, 
        typename ContextT::token_type const &act_token)
    {
        typedef typename ContextT::token_type token_type;
        typedef typename token_type::string_type string_type;

    bool valid_option = false;

        if (1 == values.size()) {
        token_type const &value = values.front();
        
            if (value.get_value() == "enable" ||
                value.get_value() == "on" || 
                value.get_value() == "1") 
            {
            // #pragma wave trace(enable)
                enable_tracing(static_cast<trace_flags>(
                    tracing_enabled() | trace_macros));
                valid_option = true;
            }
            else if (value.get_value() == "disable" ||
                value.get_value() == "off" || 
                value.get_value() == "0") 
            {
            // #pragma wave trace(disable)
                enable_tracing(static_cast<trace_flags>(
                    tracing_enabled() & ~trace_macros));
                valid_option = true;
            }
        }
        if (!valid_option) {
        // unknown option value
        string_type option_str ("trace");

            if (values.size() > 0) {
                option_str += "(";
                option_str += boost::wave::util::impl::as_string(values);
                option_str += ")";
            }
            BOOST_WAVE_THROW(preprocess_exception, ill_formed_pragma_option,
                option_str.c_str(), act_token.get_position());
        }
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  interpret the pragma wave option(preserve: [0|1|2]) directive
    template <typename ContextT, typename IteratorT>
    bool 
    interpret_pragma_option_preserve(ContextT &ctx, IteratorT &it,
        IteratorT end, typename ContextT::token_type const &act_token)
    {
        using namespace boost::wave;

        token_id id = util::impl::skip_whitespace(it, end);
        if (T_COLON == id)
            id = util::impl::skip_whitespace(it, end);
        
        if (T_PP_NUMBER != id) 
            return false;
            
        using namespace std;    // some platforms have atoi in namespace std
        switch(atoi((*it).get_value().c_str())) {
        case 0:   
            preserve_whitespace = false;
            ctx.set_language(
                enable_preserve_comments(ctx.get_language(), false),
                false);
            break;
            
        case 2:
            preserve_whitespace = true;
            /* fall through */
        case 1:
            ctx.set_language(
                enable_preserve_comments(ctx.get_language()),
                false);
            break;
            
        default:
            return false;
        }
        return true;
    }
    
    //  interpret the pragma wave option(line: [0|1]) directive
    template <typename ContextT, typename IteratorT>
    bool 
    interpret_pragma_option_line(ContextT &ctx, IteratorT &it,
        IteratorT end, typename ContextT::token_type const &act_token)
    {
        using namespace boost::wave;

        token_id id = util::impl::skip_whitespace(it, end);
        if (T_COLON == id)
            id = util::impl::skip_whitespace(it, end);
        
        if (T_PP_NUMBER != id) 
            return false;
            
        using namespace std;    // some platforms have atoi in namespace std
        int emit_lines = atoi((*it).get_value().c_str());
        if (0 == emit_lines || 1 == emit_lines) {
            // set the new emit #line directive mode
            ctx.set_language(
                enable_emit_line_directives(ctx.get_language(), emit_lines),
                false);
            return true;
        }
        return false;
    }

    //  interpret the pragma wave option(output: ["filename"|null]) directive
    template <typename ContextT, typename IteratorT>
    bool 
    interpret_pragma_option_output(ContextT &ctx, IteratorT &it,
        IteratorT end, typename ContextT::token_type const &act_token)
    {
        using namespace boost::wave;
        namespace fs = boost::filesystem;
        
        typedef typename ContextT::token_type token_type;
        typedef typename token_type::string_type string_type;

        token_id id = util::impl::skip_whitespace(it, end);
        if (T_COLON == id)
            id = util::impl::skip_whitespace(it, end);
        
        if (T_STRINGLIT == id) {
            namespace fs = boost::filesystem;
            
            string_type fname ((*it).get_value());
            fs::path fpath (
                util::impl::unescape_lit(fname.substr(1, fname.size()-2)).c_str(),
                fs::native);
            fpath = fs::complete(fpath, ctx.get_current_directory());
            
            // close the current file
            if (outputstrm.is_open())
                outputstrm.close();

            // ensure all directories for this file do exist
            fs::create_directories(fpath.branch_path());
            
            // figure out, whether the file to open was last accessed by us
            std::ios::openmode mode = std::ios::out;
            if (fs::exists(fpath) && fs::last_write_time(fpath) >= started_at)
                mode = (std::ios::openmode)(std::ios::out | std::ios::app);

            // open the new file
            outputstrm.open(fpath.string().c_str(), mode);
            if (!outputstrm.is_open()) { 
                BOOST_WAVE_THROW(preprocess_exception, could_not_open_output_file,
                    fpath.string().c_str(), act_token.get_position());
            }
            generate_output = true;
            return true;        
        }
        if (T_IDENTIFIER == id && (*it).get_value() == "null") {
        // suppress all output from this point on
            if (outputstrm.is_open())
                outputstrm.close();
            generate_output = false;
            return true;        
        }
        if (T_DEFAULT == id) {
        // re-open the default output given on command line
            if (outputstrm.is_open())
                outputstrm.close();
            
            if (!default_outfile.empty()) {
                if (default_outfile == "-") {
                // the output was suppressed on the command line
                    generate_output = false;
                }
                else {
                // there was a file name on the command line
                fs::path fpath(default_outfile, fs::native);
                    
                    // figure out, whether the file to open was last accessed by us
                    std::ios::openmode mode = std::ios::out;
                    if (fs::exists(fpath) && fs::last_write_time(fpath) >= started_at)
                        mode = (std::ios::openmode)(std::ios::out | std::ios::app);

                    // open the new file
                    outputstrm.open(fpath.string().c_str(), mode);
                    if (!outputstrm.is_open()) { 
                        BOOST_WAVE_THROW(preprocess_exception, 
                            could_not_open_output_file, fpath.string().c_str(), 
                            act_token.get_position());
                    }
                    generate_output = true;
                }
            }
            else {
            // generate the output to std::cout
                generate_output = true;
            }
            return true;        
        }
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  interpret the pragma wave option() directives
    template <typename ContextT, typename ContainerT>
    bool 
    interpret_pragma_option(ContextT &ctx, ContainerT const &values, 
        typename ContextT::token_type const &act_token)
    {
        using namespace boost::wave;
        
        typedef typename ContextT::token_type token_type;
        typedef typename token_type::string_type string_type;
        typedef typename ContainerT::const_iterator const_iterator;
        
        const_iterator end = values.end();
        for (const_iterator it = values.begin(); it != end; /**/) {
        bool valid_option = false;
            
            token_type const &value = *it;
            if (value.get_value() == "preserve") {
            // #pragma wave option(preserve: [0|1|2])
                valid_option = interpret_pragma_option_preserve(ctx, it, end, 
                    act_token);
            }
            else if (value.get_value() == "line") {
            // #pragma wave option(line: [0|1])
                valid_option = interpret_pragma_option_line(ctx, it, end, 
                    act_token);
            }
            else if (value.get_value() == "output") {
            // #pragma wave option(output: ["filename"|null])
                valid_option = interpret_pragma_option_output(ctx, it, end, 
                    act_token);
            }

            if (!valid_option) {
            // unknown option value
            string_type option_str ("option");

                if (values.size() > 0) {
                    option_str += "(";
                    option_str += util::impl::as_string(values);
                    option_str += ")";
                }
                BOOST_WAVE_THROW(preprocess_exception, ill_formed_pragma_option,
                    option_str.c_str(), act_token.get_position());
            }
            
            token_id id = util::impl::skip_whitespace(it, end);
            if (id == T_COMMA)
                util::impl::skip_whitespace(it, end);
        }
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // interpret the #pragma wave system() directive
    template <typename ContextT, typename ContainerT>
    bool
    interpret_pragma_system(ContextT const &ctx, ContainerT &pending, 
        ContainerT const &values, 
        typename ContextT::token_type const &act_token)
    {
        typedef typename ContextT::token_type token_type;
        typedef typename token_type::string_type string_type;

        if (0 == values.size()) return false;   // ill_formed_pragma_option
        
    string_type stdout_file(std::tmpnam(0));
    string_type stderr_file(std::tmpnam(0));
    string_type system_str(boost::wave::util::impl::as_string(values));
    string_type native_cmd(system_str);

        system_str += " >" + stdout_file + " 2>" + stderr_file;
        if (0 != std::system(system_str.c_str())) {
        // unable to spawn the command
        string_type error_str("unable to spawn command: ");
        
            error_str += native_cmd;
            BOOST_WAVE_THROW(preprocess_exception, ill_formed_pragma_option,
                error_str.c_str(), act_token.get_position());
        }
        
    // rescan the content of the stdout_file and insert it as the 
    // _Pragma replacement
        typedef typename ContextT::lexer_type lexer_type;
        typedef typename ContextT::input_policy_type input_policy_type;
        typedef boost::wave::iteration_context<lexer_type, input_policy_type> 
            iteration_context_type;

    iteration_context_type iter_ctx(stdout_file.c_str(), 
        act_token.get_position(), ctx.get_language());
    ContainerT pragma;

        for (/**/; iter_ctx.first != iter_ctx.last; ++iter_ctx.first) 
            pragma.push_back(*iter_ctx.first);

    // prepend the newly generated token sequence to the 'pending' container
        pending.splice(pending.begin(), pragma);

    // erase the created tempfiles
        std::remove(stdout_file.c_str());
        std::remove(stderr_file.c_str());
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  The function enable_tracing is called, whenever the status of the 
    //  tracing was changed.
    //  The parameter 'enable' is to be used as the new tracing status.
    void enable_tracing(trace_flags flags) 
        { logging_flags = flags; }

    //  The function tracing_enabled should return the current tracing status.
    trace_flags tracing_enabled() 
        { return logging_flags; }

    //  Helper functions for generating the trace output
    void open_trace_body(char const *label = 0)
    {
        if (label)
            output(label);
        output("[\n");
        increment_level();
    }
    void close_trace_body()
    {
        if (get_level() > 0) {
            decrement_level();
            output("]\n");
            tracestrm << std::flush;      // flush the stream buffer
        }
    }

    template <typename StringT>
    void output(StringT const &outstr) const
    {
        indent(get_level());
        tracestrm << outstr;          // output the given string
    }

    void indent(int level) const
    {
        for (int i = 0; i < level; ++i)
            tracestrm << "  ";        // indent
    }

    int increment_level() { return ++level; }
    int decrement_level() { BOOST_ASSERT(level > 0); return --level; }
    int get_level() const { return level; }
    
    bool enabled_macro_tracing() const 
    { 
        return (flags & trace_macros) && (logging_flags & trace_macros); 
    }
    bool enabled_include_tracing() const 
    { 
        return (flags & trace_includes); 
    }
    
    void timer(TokenT const &value)
    {
        if (value.get_value() == "0" || value.get_value() == "restart") {
        // restart the timer
            elapsed_time.restart();
        }
        else if (value.get_value() == "1") {
        // print out the current elapsed time
            std::cerr 
                << value.get_position() << ": " 
                << elapsed_time.format_elapsed_time()
                << std::endl;
        }
        else if (value.get_value() == "suspend") {
        // suspend the timer
            elapsed_time.suspend();
        }
        else if (value.get_value() == "resume") {
        // resume the timer
            elapsed_time.resume();
        }
    }

private:
    std::ofstream &outputstrm;      // main output stream
    std::ostream &tracestrm;        // trace output stream
    std::ostream &includestrm;      // included list output stream
    int level;                      // indentation level
    trace_flags flags;              // enabled globally
    trace_flags logging_flags;      // enabled by a #pragma
    bool enable_system_command;     // enable #pragma wave system() command
    bool preserve_whitespace;       // enable whitespace preservation
    bool& generate_output;          // allow generated tokens to be streamed to output
    std::string const& default_outfile;    // name of the output file given on command line
    
    stop_watch elapsed_time;        // trace timings
    std::time_t started_at;         // time, this process was started at
};

#undef BOOST_WAVE_GETSTRING
#undef BOOST_WAVE_OSSTREAM

#endif // !defined(TRACE_MACRO_EXPANSION_HPP_D8469318_8407_4B9D_A19F_13CA60C1661F_INCLUDED)
