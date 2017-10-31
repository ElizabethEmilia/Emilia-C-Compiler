#include "ppparser.h"
#include "common/console.h"

namespace Miyuki::Parse {
    void PreprocessorParser::testLexer() {
        Lex::Token::flread = M_pplex->getSourceManager();
        next();
        //cout << (int)look->tag;
        while (look->isNot(-1)) {
            cout << Console::Cyan(__FUNCTION__) << look->toString() << endl;
            next();
        }
        parseDone();
    }

    void PreprocessorParser::parse() {
        /*while (getCache()) {
            cachedLine = eval(cachedLine);
            cout << Console::Yellow("Cache Got **************") << endl;
            for (int i=0; i<cachedLine->size(); i++) {
                cout << "Test:  " << (*cachedLine)[i]->toString() << endl;
            }
            cout << endl;
        }*/
        Lex::Token::flread = M_pplex->getSourceManager();
        while (1) {
            evalCachedLine();
            if (!evaledToks || evaledToks->size() == 0)  break;
            cout << Console::Yellow("Cache Got **************") << endl;
            for (int i=0; i<evaledToks->size(); i++) {
                cout << "Test:  " << (*evaledToks)[i]->toString() << endl;
            }
            cout << endl;
        }
        parseDone();
    }

    int FunctionLikeMacro::replace(TokenSequence &toksResult) {
        int replacementCount = 0;
        if (((!defination->isParamVarible) && defination->lparlen.size() != params.size()) || (defination->isParamVarible && defination->lparlen.size() > params.size())) {
            IParser::instance->diagError("'{0}' requires {1}{2} parameters, and {3} provided."_format( macroName->toSourceLiteral(), defination->isParamVarible ? "at least ":"", defination->lparlen.size() , params.size() ), macroName);
            return 0;
        }
        size_t replen = defination->replacement.size();
        for (int i=0; i<replen; i++) {
            TokenPtr tok = defination->replacement[i];

            //  SHARP to string
            if (tok->is('#')) {
                // get next token
                WordTokenPtr tok = dynamic_pointer_cast<WordToken>( defination->replacement[++i] );
                if (!(tok && isParameter( tok->name ))) {
                    IParser::instance->diagError("'#' is not followed by a macro parameter", tok);
                    continue;
                }
                // generate string from name
                // this is macro parameter name
                string paramName = tok->name;
                // then through the paramName from the incoming parameters to find TokenSequence
                // aka parameter value
                TokenSequencePtr tokSeq = getParameterValue(paramName);
                // if is # just convert to string directly, do not do any other convertion
                string strRet;
                for (int k=0; k<tokSeq->size(); k++) {
                    strRet += (*tokSeq)[k]->toSourceLiteral();
                    if (k != tokSeq->size() - 1) strRet += " ";
                }
                toksResult.push_back(make_shared<StringToken>( strRet,  Encoding::ASCII ));
                replacementCount++;
                continue;
            }

            // DOUBLE SHARP paste
            //   directly paste x of ##x on previous token
            if (tok->is(Tag::DoubleSharp)) {
                if (i == 0 || i == replen - 1) {
                    IParser::instance->diagError("'##' cannot at either start or end of the expansion", tok);
                    continue;
                }
                WordTokenPtr tok = dynamic_pointer_cast<WordToken>( defination->replacement[++i] );

                // PASTE token on last token
                // if last token is identifier
                TokenPtr lastTok = toksResult.back();
                WordTokenPtr wordPtr = make_shared<WordToken>("");
                toksResult.pop_back();
                if ( dynamic_pointer_cast<WordToken>( lastTok ) || dynamic_pointer_cast<PPNumberToken>( lastTok ) || dynamic_pointer_cast<PPLiteralToken>( lastTok ) ) {
                    wordPtr->name = lastTok->toSourceLiteral();
                }
                else {
                    IParser::instance->diagError("invalid use of '##' ", tok);
                    toksResult.push_back(lastTok);  // this token is invalid, put back
                    continue;
                }
                if (tok && isParameter( tok->name )) {
                    //get parameter value
                    TokenSequencePtr tokSeq = getParameterValue(tok->name);
                    //Is a complete Token, e.g. 123
                    // directly paste on last token
                    wordPtr->name += (*tokSeq)[0]->toSourceLiteral();
                    toksResult.push_back(wordPtr);
                    if (tokSeq->size() == 1) ;//do nothing
                        // else insert new token
                    else {
                        for (int i=1; i<tokSeq->size(); i++)
                            toksResult.push_back((*tokSeq)[i]);
                    }
                }
                else {//directly paste literal value
                    wordPtr->name += tok->name;
                    toksResult.push_back(wordPtr);
                }
                replacementCount++;
                continue;
            }

            // if is parameter name
            WordTokenPtr tokW = dynamic_pointer_cast<WordToken>(tok);
            if ( tokW && isParameter( tokW->name ) ) {
                TokenSequencePtr tokSeq = getParameterValue(tokW->name);
                for (int i=0; i<tokSeq->size(); i++)
                    toksResult.push_back((*tokSeq)[i]);
                replacementCount++;
                continue;
            }

            // else directly write
            toksResult.push_back(tok);
        }
        return replacementCount;
    }

    TokenSequencePtr FunctionLikeMacro::getParameterValue(string &name) {
        TokenSequencePtr tokSeq = make_shared<TokenSequence>();

        if (name == "__VA_ARGS__") {
            // defination->lparlen.size() = named size
            // params.size() = call size
            for (int i=defination->lparlen.size(); i<params.size(); i++) {
                for (TokenPtr ptr : *(params[i])) {
                    tokSeq->push_back(ptr);
                }
                tokSeq->push_back(make_shared<Token>(','));
            }
            if (tokSeq->size())
                tokSeq->pop_back(); //remove last comma token
            return tokSeq;
        }
        int index = defination->getParamIndex(name);
        if (index == -1)   return nullptr;
        return params[index];
    }

    TokenSequencePtr PreprocessorParser::eval(TokenSequencePtr original) {
        TokenSequencePtr seqNew;

        // first, we relace macros
        int replaceCount;
        do {
            replaceCount = 0;
            seqNew = make_shared<TokenSequence>();
            for (int i=0; i<original->size(); i++) {
                TokenPtr tok = (*original)[i];
                if (tok->isNot(Tag::Identifier)) {
                    // directly add to seqNew
                    seqNew->push_back(tok);
                    continue;
                }
                // else is identifier
                //   forst we should get to kow if this identifier is which one of the follow
                //   all possible types are:  macro,  function-like macro,  norml identifier(will be nnot reolaced)
                WordTokenPtr tokW = dynamic_pointer_cast<WordToken>(tok);
                // zero, we check if it is a predefined macro
                // replace line number (int)
                if (tokW->name == "__LINE__")   { seqNew->push_back(make_shared<IntToken>( Token::flread->getRow(), false ));  replaceCount++; goto replace_done; }
                    // replace file name (string-literal)
                else if (tokW->name == "__FILE__")  { seqNew->push_back(make_shared<StringToken>( Token::flread->getCurrentFilename().c_str(), Encoding::ASCII ));   replaceCount++; goto replace_done; }
                    // replace function name (string literal) //FIXME: after implement parser, write here
                else if (tokW->name == "__FUNC__")  { seqNew->push_back(make_shared<StringToken>( "to be implemented.", Encoding::ASCII ));  replaceCount++; goto replace_done; }

                // first we check if it is a macro
                MacroDefinePtr macroDef = macros.getMacroDef(tokW->name);
                if (!macroDef) {
                    // is not a macro
                    // directly add to seqNew
                    seqNew->push_back(tok);
                    continue;
                }
                // if is a function-like macro
                if (macroDef->isFunctionLike) {
                    // first we check if next token is '('
                    if (i != original->size() - 1   // has next item
                        && (*original)[i + 1]->is('(')) { // is a '('
                        i++; // skip '('
                        // fetch all tokens
                        FunctionLikeMacroPtr macro = make_shared<FunctionLikeMacro>(macroDef);
                        macro->macroName = tokW;
                        int leftBracketCount = 1;
                        TokenSequencePtr param = make_shared<TokenSequence>();  // store first param (if there is)
                        while (leftBracketCount) {
                            // when ( is more than )
                            // get next token
                            tok = (*original)[++i];

                            if (tok->is(')')) {
                                // if meet a '(', left count -1
                                --leftBracketCount;
                            }
                            else if (tok->is(',')) {
                                // if meet a  comma, add last to parameter list
                                macro->params.push_back(param);
                                param = make_shared<TokenSequence>();
                                continue;
                            }
                            else if (i >= original->size()) {
                                // token runs out
                                // should not runs here
                                diagError("unexpected new-line or eof", tok);
                                assert(false && "you should not run here");
                            }
                            else {
                                // add to param's token list
                                param->push_back(tok);
                            }
                        }
                        //add the last param (if exist)
                        if (param->size()) {
                            macro->params.push_back(param);
                        }
                        // repplace function macro
                        replaceCount += macro->replace(*seqNew);
                    }
                    // if this macro is used like a varible
                    else ; // do nothing
                }
                // is not a function-like macro
                else {
                    // replace it
                    MacroPtr macro = make_shared<Macro>(macroDef);
                    macro->macroName = tokW;
                    replaceCount += macro->replace(*seqNew);
                }
            }
replace_done:
            original = seqNew;
        }
        while (replaceCount);

        // TODO: then we calculate values in if or elif

        // return generated token sequence
        return seqNew;
    }

    bool PreprocessorParser::getCache() {
recache:
        cachedLine = make_shared<TokenSequence>();
        // check first token of the line
        next();
        // tell scanner if skip scanned line
        bool skipThisLine = false;
        // if first token is a group-part
        if (look->is('#')) {
            // get op-name
            skipThisLine =!setGroupPart( next() );
            // if is '#include' set flag to tell lexer return header name token
            if (dynamic_pointer_cast<WordToken>(look) && static_pointer_cast<WordToken>(look)->name == "include")
                M_pplex->setLexingContent(PreprocessorLexer::LexingContent::Include);

            for ( next() ; ; next() ) {
                if ( look->is('\n') || look->is(EOF) ) break;
                cachedLine->push_back(look);
            }
            // if is empty line, or this line is invalid, need recache
            int needRecache = skipThisLine;
            // if reach EOF
            if ( look->is(EOF) )  {
                // maybe last line, but this line is invalid
                if ( needRecache ) cachedLine = nullptr;
                return false;
            }
            if ( needRecache ) goto recache;
            return true;
        }

        else if (look->is(EOF)) {
            cachedLine = nullptr;
            groupPart = nullptr;
            return false;
        }

        // else is plain text-line
        groupPart = make_shared<GroupPart>(GroupPart::TextLine);
        int leftBracketCount = 0;
        bool isInFunction = true;
        for ( ; ; next() ) {
            if (look->is('\n')) {
                // for Multi-line function call, ignore this new-line
                if (isInFunction && leftBracketCount > 0) continue;
                // FIXME: add an addition 0
                break;
            }
                // readch end-of-file, Unconditional stop caching
            else if (look->is(EOF)) break;  // FIXME: add an addition 0
            cachedLine->push_back(look);
            if (look->is(Tag::Identifier)) {
                // we should not only read one more one token
                // if we found the next token is bew-lien new-line we should continue read
                // and get real meanningful token we want
                TokenPtr tok = next(), tokN = tok;
                if (tok->is('\n')) {
                    while (tok->is('\n')) {
                        // skip all new-line tokens
                        // get these token with no-caching
                        tok = M_lex->scan();
                    }
                    // find first non-new-line token
                    if (tok->is('('))  ;  // if itis s left-bracket, do nothing, fall
                    else {
                        // put this token to cache, and retract
                        //    for we read 2 more tokens ( new-line and the 'tok' ) then needed
                        cacheToken(tokN); cacheToken(tok);  retract(); retract();
                        continue;
                    }
                }
                // is in function
                if (tok->is('(')) {
                    isInFunction = true;
                    leftBracketCount++;
                    cachedLine->push_back(tok);
                    continue;
                }
                retract();
                continue;
            }
            else if (look->is(')')) {
                leftBracketCount--;
                continue;
            }
        }

        // if no token in sequence
        if ( cachedLine->size() == 0 ) {
            // meet end-of-file
            if ( look->is(EOF) )  {
                cachedLine = nullptr;
                return false;
            }
            // read next logical line
            goto recache;
        }

        return true;
    }

    bool PreprocessorParser::setGroupPart(TokenPtr op) {
        // note: if error occurred in this function,
        //  after receivng false return value, just skip the whole logical line,
        // and do not do any other processing
        int kind = -1;
        if (op->isNot(Tag::Identifier)) {
            groupPart = nullptr;
            if (op->is('\n') || op->is(EOF)) {
                // FOR control-line: # new-line
                return true;  // this struct is allowed in standard, so return true
            }
            // invalid
            return false;
        }
        WordTokenPtr opTok = static_pointer_cast<WordToken>(op);
        // Include = 0, If, Ifndef, Ifdef, Elif, Else, Endif, Define, Undef, Line, Error, Pragma, Empty, TextLine
        if (opTok->name == "include") kind = GroupPart::Include;
        else if (opTok->name == "if") kind = GroupPart::If;
        else if (opTok->name == "ifndef") kind = GroupPart::Ifndef;
        else if (opTok->name == "ifdef") kind = GroupPart::Ifdef;
        else if (opTok->name == "elif") kind = GroupPart::Elif;
        else if (opTok->name == "else") kind = GroupPart::Else;
        else if (opTok->name == "endif") kind = GroupPart::Endif;
        else if (opTok->name == "define") kind = GroupPart::Define;
        else if (opTok->name == "undef") kind = GroupPart::Undef;
        else if (opTok->name == "line") kind = GroupPart::Line;
        else if (opTok->name == "error") kind = GroupPart::Error;
        else if (opTok->name == "pragma") kind = GroupPart::Pragma;
        if (kind == -1) {
            groupPart = nullptr;
            return false;
        }
        groupPart = make_shared<GroupPart>(kind);
        groupPart->directiveTok = op;
        return true;
    }

    void PreprocessorParser::evalCachedLine() {
        do {
            cachedLine = nullptr;
            evaledToks = nullptr;
            getCache();
            if (!cachedLine)
                break;

            // is an empty sentense or invalid, who knows?
            if (!groupPart)
                continue;

            int kind = groupPart->kind;
            if (kind == GroupPart::Include) {
                processInclude();
            }
            else if (kind == GroupPart::Define) {
                processDefine();
            }
            else if (kind == GroupPart::Undef) {
                processUndef();
            }
            else if (kind == GroupPart::TextLine) {
                processTextline();
            }
            else if (kind == GroupPart::Error) {
                processError();
            }

            if (evaledToks && evaledToks->size()) evaledToksIter = evaledToks->begin();
        }
        while (!evaledToks || !evaledToks->size());
    }

#define nextTok() ( tok = (*cachedLine)[++i] )
#define currTok() ( tok = (*cachedLine)[i] )
#define noMoreTok() ( i+1 >= cachedLine->size() )
#define hasMoreToken() ( i+1 < cachedLine->size() )
#define getTok(i) ( (*cachedLine)[i] )
#define errorNoMoreToken() if (noMoreTok()) { diagError("unexpected new-line", tok); return; }
    void PreprocessorParser::processInclude() {
        cachedLine = eval(cachedLine);
        // check format
        //   headerName or stringLiteral
        if (cachedLine->size() == 1 && ((*cachedLine)[0]->is(Tag::StringLiteral) || (*cachedLine)[0]->is(Tag::HeaderName))) {
            string name = dynamic_pointer_cast<HeaderToken>((*cachedLine)[0]) ? dynamic_pointer_cast<HeaderToken>((*cachedLine)[0])->name :
                          ( dynamic_pointer_cast<StringToken>((*cachedLine)[0]) ? dynamic_pointer_cast<StringToken>((*cachedLine)[0])->value : "<file not avaible>" );
            // here set to defaultContent is import
            // I set this default value when I meet a new line,
            // but If an #include was not end with \n, lexingContent will not be set to default
            // and the lexer will lex defauleContent as Preprocess line, even a include groupPart
            // so this line is very important
            //// CELEBRATE !! FOUND THE BUG !!!!!
            M_pplex->setLexingContent(PreprocessorLexer::LexingContent::DefaultContent);

            try {
                M_lex->openFile(name.c_str());
                // TODO: undate line number
            }
            catch  (IOException& e) {
                diagError(e.what(), (*cachedLine)[0]); //filename token
            }
        }
        else diagError("file name expected.", groupPart->directiveTok);
    }

    void PreprocessorParser::processDefine() {
        TokenPtr tok;
        // CHECK FORMAT
        //   #define name [ ( param , ...
        if (cachedLine->size() < 1) {
            diagError("no macro name given in #define directive", groupPart->directiveTok);
            return;
        }
        if ((*cachedLine)[0]->isNot(Tag::Identifier)) {
            diagError("macro names must be identifiers", groupPart->directiveTok);
            return;
        }
        bool isAFunctionLikeMacro = cachedLine->size() >= 2 && (*cachedLine)[1]->is('(');
        MacroDefinePtr macroDef = make_shared<MacroDefine>();
        macroDef->isFunctionLike = isAFunctionLikeMacro;
        macroDef->isParamVarible = false;
        bool paramListClosed = false;
        if (isAFunctionLikeMacro) {
            int i = 2;
            // parse paramter list
            for ( ; i < cachedLine->size(); i++ ) {
                currTok();
                if (tok->is(Tag::Ellipsis)) {
                    macroDef->isParamVarible = true;
                    errorNoMoreToken();
                    if (nextTok()->isNot(')')) {
                        diagError("missing ')' after {0} token"_format( tok->toSourceLiteral() ), tok);
                        return;
                    }
                    paramListClosed = true;
                    break;
                }
                if (tok->isNot(Tag::Identifier)) {
                    diagError("{0} may not appear in macro parameter list"_format( tok->toSourceLiteral() ), tok);
                    return; // give up this line, similarly hereinafter
                }
                // add to parameter list
                macroDef->lparlen.push_back(dynamic_pointer_cast<WordToken>(tok));
                errorNoMoreToken();
                if (nextTok()->is(')')) {
                    paramListClosed = true;
                    break;
                }
                if (tok->isNot(',') ) {
                    diagError("macro parameters must be comma-separated", tok);
                    return;
                }
            }
            // if param list is not closed
            if (!paramListClosed) {
                tok = getTok(i-1);
                diagError("')' required after {0} token"_format(tok->toSourceLiteral()), tok);
                return;
            }
            // parse replacement-list
            for ( i++ ; i < cachedLine->size(); i++ ) {
                currTok();
                macroDef->replacement.push_back(tok);
            }
        }
        else { // isAFunctionLikeMacro
            int i = 1;
            for ( ; i < cachedLine->size(); i++ ) {
                currTok();
                macroDef->replacement.push_back(tok);
            }
        }
        // add to macro pack
        if (!macros.addMacro(static_pointer_cast<WordToken>(getTok(0))->name, macroDef)) {
            diagError("'{0}' redefined"_format(tok->toSourceLiteral()), getTok(1));
            return;
        }
    }

    void PreprocessorParser::processUndef() {
        TokenPtr tok;
        // CHECK FORMAT
        //   #undef name
        if (cachedLine->size() < 1) {
            diagError(" no macro name given in #undef directive", groupPart->directiveTok);
            return;
        }
        tok = getTok(0);
        if (tok->isNot(Tag::Identifier)) {
            diagError("macro names must be identifiers", tok);
            return;
        }
        macros.removeMacroDef(tok->toSourceLiteral()); // same as cast to WordTokenPtr and access name
    }

    void PreprocessorParser::processTextline() {
        evaledToks = eval(cachedLine);
    }

    void PreprocessorParser::processError() {
        cachedLine = eval(cachedLine);
        string errmsg;
        for (TokenPtr ptr : *cachedLine)
            errmsg += ptr->toSourceLiteral() + " ";
        diagError(std::move(errmsg), groupPart->directiveTok);
    }
}


