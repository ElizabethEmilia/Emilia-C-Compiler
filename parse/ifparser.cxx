#include "ifparser.h"
#include "common/console.h"

namespace Miyuki::Parse {

    void IParser::match(uint32_t term, string &&errmsg, TokenPtr &ptr) {
        if (look->isNot(term)) {
            diagError(std::move(errmsg), ptr);
            if (look->is(-1))  parseDone();
            if (!skipUntil({ ';' }, RecoveryFlag::KeepSpecifiedToken | RecoveryFlag::SkipUntilSemi))
                parseDone();
            return;
        }
        next();
    }

    TokenPtr IParser::next() {
reget_token:
        if (m_tsptr_r == m_tsptr_w) {
            try {
                look = M_lex->scan();
                tokens[(++m_tsptr_w) % MaxRetractSize] = look;
                m_tsptr_r++;
            }
            catch (SyntaxError& err) {
                // notice: we cannot pass look token, because lexer do not generate
                //   token at all if there's lexical error, so generate a token for
                //   error mamually.
                diagError(err.what(), M_lex->getLexedInvalidToken());
                goto reget_token;
            }
            return look;
        }
        else if (m_tsptr_r < m_tsptr_w) {
            look = tokens[(++m_tsptr_r) % MaxRetractSize];
            return look;
        }
        else assert( false && "stack overflow." );
    }

    TokenPtr IParser::retract() {
        if (m_tsptr_w - m_tsptr_r > MaxRetractSize || m_tsptr_r <= 0)
            assert( false && "Invaild retract operation." );
        look = tokens[(--m_tsptr_r) % MaxRetractSize];
        return look;
    }

    void IParser::reportError(std::ostream& os) {
        for (ParseError& e : errors) {
            TokenPtr tok = e.getToken();
            string s = M_lex->getSourceManager()->getLine(tok->filenam, tok->row);
            for (int i=0 ; i < s.length() ; i++)
                    if (s[i] != '\n')  cout << s[i];
            cout  << endl;
            for (int i=0; i<s.length(); i++)
                if (s[i] == '\t') cout << "\t";
            for (int i=1; i<tok->startCol; i++)
                os << " ";
            os << "^";
            for (int i=1; i<tok->chrlen; i++)
                os << "~";
            os << endl;
            if (e.isWarning())  os << Miyuki::Console::Warning();
            else os << Miyuki::Console::Error();
            os << tok->filenam << ":" << tok->row << ":" << tok->column << ": " << e.what() << endl << endl;
        }
    }

    void IParser::parseDone() {
        reportError(cout);
        if ( errors.size() != 0 )  throw PasreCannotRecoveryException();
    }

    bool IParser::skipUntil(const deque<int32_t>& toks, uint32_t flag) {
        // First we find token we want
        for ( ; ; next() ) {
            for (int32_t expect : toks ) {
                if (look->is(expect)) {
                    if (hasRecoveryFlag(flag, RecoveryFlag::KeepSpecifiedToken));// do nothing
                    else next();
                    return true;
                }
            }

            if ( hasRecoveryFlag(flag, RecoveryFlag::SkipUntilSemi) && look->is(';') )
                return false;

            // meet EOF and is required
            if ( look->is(-1) && toks.size() == 1 && toks[0] == -1)
                return true;

            // token runs out
            if ( look->is(-1) )
                return false;

            // TODO: add_special_skip_rules
        }
    }
}

