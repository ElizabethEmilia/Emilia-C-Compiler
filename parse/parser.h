#ifndef _MIYUKI_PARSER_H
#define _MIYUKI_PARSER_H

#include "parse/ifparser.h"
#include "parse/ppparser.h"

namespace Miyuki::Parse {

    class Parser : public IParser {
    private:
        // preprocessor: we get token from it
        PreprocessorPtr M_proc;

        // generated AST
        TokenPtr        M_ast = nullptr;

        // get next token
        //    get token from preprocessor
        TokenPtr next() override;

    public:
        explicit  Parser(const char * path);
        
        // parse source code
        //   returns: root of AST
        void parse() override;

        // get preprocessed source file
        TokenSequencePtr getPreprocessedSource();
    };

}

#endif