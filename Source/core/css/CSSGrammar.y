%{

/*
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *  Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSParserMode.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryExp.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "wtf/FastMalloc.h"
#include <stdlib.h>
#include <string.h>

using namespace WebCore;
using namespace HTMLNames;

#define YYMALLOC fastMalloc
#define YYFREE fastFree

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 1
#define YYMAXDEPTH 10000
#define YYDEBUG 0

#if YYDEBUG > 0
#define YYPRINT(File,Type,Value) if (isCSSTokenAString(Type)) YYFPRINTF(File, "%s", String((Value).string).utf8().data())
#endif

%}

%pure_parser

%parse-param { BisonCSSParser* parser }
%lex-param { BisonCSSParser* parser }

%union {
    bool boolean;
    char character;
    int integer;
    double number;
    CSSParserString string;

    StyleRuleBase* rule;
    // The content of the two below HeapVectors are guaranteed to be kept alive by
    // the corresponding m_parsedRules and m_floatingMediaQueryExpList lists in BisonCSSParser.h.
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase> >* ruleList;
    WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp> >* mediaQueryExpList;
    CSSParserSelector* selector;
    Vector<OwnPtr<CSSParserSelector> >* selectorList;
    CSSSelector::MarginBoxType marginBox;
    CSSSelector::Relation relation;
    MediaQuerySet* mediaList;
    MediaQuery* mediaQuery;
    MediaQuery::Restrictor mediaQueryRestrictor;
    MediaQueryExp* mediaQueryExp;
    CSSParserValue value;
    CSSParserValueList* valueList;
    StyleKeyframe* keyframe;
    Vector<RefPtr<StyleKeyframe> >* keyframeRuleList;
    float val;
    CSSPropertyID id;
    CSSParserLocation location;
}

%{

static inline int cssyyerror(void*, const char*)
{
    return 1;
}

#if YYDEBUG > 0
static inline bool isCSSTokenAString(int yytype)
{
    switch (yytype) {
    case IDENT:
    case STRING:
    case NTH:
    case HEX:
    case IDSEL:
    case DIMEN:
    case INVALIDDIMEN:
    case URI:
    case FUNCTION:
    case ANYFUNCTION:
    case HOSTFUNCTION:
    case ANCESTORFUNCTION:
    case NOTFUNCTION:
    case CALCFUNCTION:
    case MINFUNCTION:
    case MAXFUNCTION:
    case UNICODERANGE:
        return true;
    default:
        return false;
    }
}
#endif

inline static CSSParserValue makeOperatorValue(int value)
{
    CSSParserValue v;
    v.id = CSSValueInvalid;
    v.unit = CSSParserValue::Operator;
    v.iValue = value;
    return v;
}

inline static CSSParserValue makeIdentValue(CSSParserString string)
{
    CSSParserValue v;
    v.id = cssValueKeywordID(string);
    v.unit = CSSPrimitiveValue::CSS_IDENT;
    v.string = string;
    return v;
}

%}

%expect 0

%nonassoc LOWEST_PREC

%left UNIMPORTANT_TOK

%token WHITESPACE SGML_CD
%token TOKEN_EOF 0

%token INCLUDES
%token DASHMATCH
%token BEGINSWITH
%token ENDSWITH
%token CONTAINS

%token <string> STRING
%right <string> IDENT
%token <string> NTH

%nonassoc <string> HEX
%nonassoc <string> IDSEL
%nonassoc ':'
%nonassoc '.'
%nonassoc '['
%nonassoc <string> '*'
%nonassoc error
%left '|'

%token IMPORT_SYM
%token PAGE_SYM
%token MEDIA_SYM
%token SUPPORTS_SYM
%token FONT_FACE_SYM
%token CHARSET_SYM
%token NAMESPACE_SYM
%token VIEWPORT_RULE_SYM
%token INTERNAL_DECLS_SYM
%token INTERNAL_MEDIALIST_SYM
%token INTERNAL_RULE_SYM
%token INTERNAL_SELECTOR_SYM
%token INTERNAL_VALUE_SYM
%token INTERNAL_KEYFRAME_RULE_SYM
%token INTERNAL_KEYFRAME_KEY_LIST_SYM
%token INTERNAL_SUPPORTS_CONDITION_SYM
%token KEYFRAMES_SYM
%token WEBKIT_KEYFRAMES_SYM
%token <marginBox> TOPLEFTCORNER_SYM
%token <marginBox> TOPLEFT_SYM
%token <marginBox> TOPCENTER_SYM
%token <marginBox> TOPRIGHT_SYM
%token <marginBox> TOPRIGHTCORNER_SYM
%token <marginBox> BOTTOMLEFTCORNER_SYM
%token <marginBox> BOTTOMLEFT_SYM
%token <marginBox> BOTTOMCENTER_SYM
%token <marginBox> BOTTOMRIGHT_SYM
%token <marginBox> BOTTOMRIGHTCORNER_SYM
%token <marginBox> LEFTTOP_SYM
%token <marginBox> LEFTMIDDLE_SYM
%token <marginBox> LEFTBOTTOM_SYM
%token <marginBox> RIGHTTOP_SYM
%token <marginBox> RIGHTMIDDLE_SYM
%token <marginBox> RIGHTBOTTOM_SYM

%token ATKEYWORD

%token IMPORTANT_SYM
%token MEDIA_ONLY
%token MEDIA_NOT
%token MEDIA_AND
%token MEDIA_OR

%token SUPPORTS_NOT
%token SUPPORTS_AND
%token SUPPORTS_OR

%token <number> REMS
%token <number> CHS
%token <number> QEMS
%token <number> EMS
%token <number> EXS
%token <number> PXS
%token <number> CMS
%token <number> MMS
%token <number> INS
%token <number> PTS
%token <number> PCS
%token <number> DEGS
%token <number> RADS
%token <number> GRADS
%token <number> TURNS
%token <number> MSECS
%token <number> SECS
%token <number> HERTZ
%token <number> KHERTZ
%token <string> DIMEN
%token <string> INVALIDDIMEN
%token <number> PERCENTAGE
%token <number> FLOATTOKEN
%token <number> INTEGER
%token <number> VW
%token <number> VH
%token <number> VMIN
%token <number> VMAX
%token <number> DPPX
%token <number> DPI
%token <number> DPCM
%token <number> FR

%token <string> URI
%token <string> FUNCTION
%token <string> ANYFUNCTION
%token <string> CUEFUNCTION
%token <string> NOTFUNCTION
%token <string> DISTRIBUTEDFUNCTION
%token <string> CALCFUNCTION
%token <string> MINFUNCTION
%token <string> MAXFUNCTION
%token <string> HOSTFUNCTION
%token <string> ANCESTORFUNCTION

%token <string> UNICODERANGE

%type <relation> combinator

%type <rule> ruleset
%type <rule> media
%type <rule> import
%type <rule> namespace
%type <rule> page
%type <rule> margin_box
%type <rule> font_face
%type <rule> keyframes
%type <rule> rule
%type <rule> valid_rule
%type <ruleList> block_rule_body
%type <ruleList> block_rule_list
%type <rule> block_rule
%type <rule> block_valid_rule
%type <rule> supports
%type <rule> viewport
%type <boolean> keyframes_rule_start

%type <string> maybe_ns_prefix

%type <string> namespace_selector

%type <string> string_or_uri
%type <string> ident_or_string
%type <string> medium
%type <marginBox> margin_sym

%type <mediaList> media_list
%type <mediaList> maybe_media_list
%type <mediaList> mq_list
%type <mediaQuery> media_query
%type <mediaQuery> valid_media_query
%type <mediaQueryRestrictor> maybe_media_restrictor
%type <valueList> maybe_media_value
%type <mediaQueryExp> media_query_exp
%type <mediaQueryExpList> media_query_exp_list
%type <mediaQueryExpList> maybe_and_media_query_exp_list

%type <boolean> supports_condition
%type <boolean> supports_condition_in_parens
%type <boolean> supports_negation
%type <boolean> supports_conjunction
%type <boolean> supports_disjunction
%type <boolean> supports_declaration_condition

%type <string> keyframe_name
%type <keyframe> keyframe_rule
%type <keyframeRuleList> keyframes_rule
%type <keyframeRuleList> keyframe_rule_list
%type <valueList> key_list
%type <value> key

%type <id> property

%type <selector> specifier
%type <selector> specifier_list
%type <selector> simple_selector
%type <selector> selector
%type <selector> relative_selector
%type <selectorList> selector_list
%type <selectorList> simple_selector_list
%type <selector> class
%type <selector> attrib
%type <selector> pseudo
%type <selector> pseudo_page
%type <selector> page_selector

%type <boolean> declaration_list
%type <boolean> decl_list
%type <boolean> declaration
%type <boolean> declarations_and_margins

%type <boolean> prio

%type <integer> match
%type <integer> unary_operator
%type <integer> maybe_unary_operator
%type <character> operator

%type <valueList> expr
%type <value> term
%type <value> unary_term
%type <value> function
%type <value> calc_func_term
%type <character> calc_func_operator
%type <valueList> calc_func_expr
%type <valueList> calc_func_expr_list
%type <valueList> calc_func_paren_expr
%type <value> calc_function
%type <string> min_or_max
%type <value> min_or_max_function

%type <string> element_name
%type <string> attr_name

%type <location> error_location

%type <valueList> ident_list
%type <value> track_names_list

%%

stylesheet:
    maybe_charset maybe_sgml rule_list
  | internal_decls
  | internal_rule
  | internal_selector
  | internal_value
  | internal_medialist
  | internal_keyframe_rule
  | internal_keyframe_key_list
  | internal_supports_condition
  ;

internal_rule:
    INTERNAL_RULE_SYM maybe_space valid_rule maybe_space TOKEN_EOF {
        parser->m_rule = $3;
    }
;

internal_keyframe_rule:
    INTERNAL_KEYFRAME_RULE_SYM maybe_space keyframe_rule maybe_space TOKEN_EOF {
        parser->m_keyframe = $3;
    }
;

internal_keyframe_key_list:
    INTERNAL_KEYFRAME_KEY_LIST_SYM maybe_space key_list TOKEN_EOF {
        parser->m_valueList = parser->sinkFloatingValueList($3);
    }
;

internal_decls:
    INTERNAL_DECLS_SYM maybe_space_before_declaration declaration_list TOKEN_EOF {
        /* can be empty */
    }
;

internal_value:
    INTERNAL_VALUE_SYM maybe_space expr TOKEN_EOF {
        parser->m_valueList = parser->sinkFloatingValueList($3);
        int oldParsedProperties = parser->m_parsedProperties.size();
        if (!parser->parseValue(parser->m_id, parser->m_important))
            parser->rollbackLastProperties(parser->m_parsedProperties.size() - oldParsedProperties);
        parser->m_valueList = nullptr;
    }
;

internal_medialist:
    INTERNAL_MEDIALIST_SYM maybe_space location_label maybe_media_list TOKEN_EOF {
        parser->m_mediaList = $4;
    }
;

internal_selector:
    INTERNAL_SELECTOR_SYM maybe_space selector_list TOKEN_EOF {
        if (parser->m_selectorListForParseSelector)
            parser->m_selectorListForParseSelector->adoptSelectorVector(*$3);
    }
;

internal_supports_condition:
    INTERNAL_SUPPORTS_CONDITION_SYM maybe_space supports_condition TOKEN_EOF {
        parser->m_supportsCondition = $3;
    }
;

space:
    WHITESPACE
  | space WHITESPACE
  ;

maybe_space:
    /* empty */ %prec UNIMPORTANT_TOK
  | space
  ;

maybe_sgml:
    /* empty */
  | maybe_sgml SGML_CD
  | maybe_sgml WHITESPACE
  ;

closing_brace:
    '}'
  | %prec LOWEST_PREC TOKEN_EOF
  ;

closing_parenthesis:
    ')'
  | %prec LOWEST_PREC TOKEN_EOF
  ;

closing_square_bracket:
    ']'
  | %prec LOWEST_PREC TOKEN_EOF
  ;

semi_or_eof:
    ';'
  | TOKEN_EOF
  ;

maybe_charset:
    /* empty */
  | CHARSET_SYM maybe_space STRING maybe_space semi_or_eof {
       if (parser->m_styleSheet)
           parser->m_styleSheet->parserSetEncodingFromCharsetRule($3);
       parser->startEndUnknownRule();
    }
  | CHARSET_SYM at_rule_recovery
  ;

rule_list:
   /* empty */
 | rule_list rule maybe_sgml {
     if ($2 && parser->m_styleSheet)
         parser->m_styleSheet->parserAppendRule($2);
 }
 ;

valid_rule:
    ruleset
  | media
  | page
  | font_face
  | keyframes
  | namespace
  | import
  | supports
  | viewport
  ;

before_rule:
    /* empty */ {
        parser->startRule();
    }
  ;

rule:
    before_rule valid_rule {
        $$ = $2;
        parser->m_hadSyntacticallyValidCSSRule = true;
        parser->endRule(!!$$);
    }
  | before_rule invalid_rule {
        $$ = 0;
        parser->endRule(false);
    }
  ;

block_rule_body:
    block_rule_list
  | block_rule_list block_rule_recovery
    ;

block_rule_list:
    /* empty */ { $$ = 0; }
  | block_rule_list block_rule maybe_sgml {
      $$ = parser->appendRule($1, $2);
    }
    ;

block_rule_recovery:
    before_rule invalid_rule_header {
        parser->endRule(false);
    }
  ;

block_valid_rule:
    ruleset
  | page
  | font_face
  | media
  | keyframes
  | supports
  | viewport
  | namespace
  ;

block_rule:
    before_rule block_valid_rule {
        $$ = $2;
        parser->endRule(!!$$);
    }
  | before_rule invalid_rule {
        $$ = 0;
        parser->endRule(false);
    }
  ;

before_import_rule:
    /* empty */ {
        parser->startRuleHeader(CSSRuleSourceData::IMPORT_RULE);
    }
    ;

import_rule_start:
    before_import_rule IMPORT_SYM maybe_space {
        parser->endRuleHeader();
        parser->startRuleBody();
    }
  ;

import:
    import_rule_start string_or_uri maybe_space location_label maybe_media_list semi_or_eof {
        $$ = parser->createImportRule($2, $5);
    }
  | import_rule_start string_or_uri maybe_space location_label maybe_media_list invalid_block {
        $$ = 0;
    }
  ;

namespace:
    NAMESPACE_SYM maybe_space maybe_ns_prefix string_or_uri maybe_space semi_or_eof {
        parser->addNamespace($3, $4);
        $$ = 0;
    }
  ;

maybe_ns_prefix:
/* empty */ { $$.clear(); }
| IDENT maybe_space
;

string_or_uri:
STRING
| URI
;

maybe_media_value:
    /*empty*/ {
        $$ = 0;
    }
    | ':' maybe_space expr {
        $$ = $3;
    }
    ;

media_query_exp:
    '(' maybe_space IDENT maybe_space maybe_media_value closing_parenthesis {
        parser->tokenToLowerCase($3);
        $$ = parser->createFloatingMediaQueryExp($3, $5);
        if (!$$)
            YYERROR;
    }
    | '(' error error_recovery closing_parenthesis {
        YYERROR;
    }
    ;

media_query_exp_list:
    media_query_exp {
        $$ = parser->createFloatingMediaQueryExpList();
        $$->append(parser->sinkFloatingMediaQueryExp($1));
    }
    | media_query_exp_list maybe_space MEDIA_AND maybe_space media_query_exp {
        $$ = $1;
        $$->append(parser->sinkFloatingMediaQueryExp($5));
    }
    ;

maybe_and_media_query_exp_list:
    maybe_space {
        $$ = parser->createFloatingMediaQueryExpList();
    }
    | maybe_space MEDIA_AND maybe_space media_query_exp_list maybe_space {
        $$ = $4;
    }
    ;

maybe_media_restrictor:
    /*empty*/ {
        $$ = MediaQuery::None;
    }
    | MEDIA_ONLY maybe_space {
        $$ = MediaQuery::Only;
    }
    | MEDIA_NOT maybe_space {
        $$ = MediaQuery::Not;
    }
    ;

valid_media_query:
    media_query_exp_list maybe_space {
        $$ = parser->createFloatingMediaQuery(parser->sinkFloatingMediaQueryExpList($1));
    }
    | maybe_media_restrictor medium maybe_and_media_query_exp_list {
        parser->tokenToLowerCase($2);
        $$ = parser->createFloatingMediaQuery($1, $2, parser->sinkFloatingMediaQueryExpList($3));
    }
    ;

media_query:
    valid_media_query
    | valid_media_query error error_location rule_error_recovery {
        parser->reportError(parser->lastLocationLabel(), InvalidMediaQueryCSSError);
        $$ = parser->createFloatingNotAllQuery();
    }
    | error error_location rule_error_recovery {
        parser->reportError(parser->lastLocationLabel(), InvalidMediaQueryCSSError);
        $$ = parser->createFloatingNotAllQuery();
    }
    ;

maybe_media_list:
    /* empty */ {
        $$ = parser->createMediaQuerySet();
    }
    | media_list
    ;

media_list:
    media_query {
        $$ = parser->createMediaQuerySet();
        $$->addMediaQuery(parser->sinkFloatingMediaQuery($1));
    }
    | mq_list media_query {
        $$ = $1;
        $$->addMediaQuery(parser->sinkFloatingMediaQuery($2));
    }
    | mq_list {
        $$ = $1;
        $$->addMediaQuery(parser->sinkFloatingMediaQuery(parser->createFloatingNotAllQuery()));
    }
    ;

mq_list:
    media_query ',' maybe_space location_label {
        $$ = parser->createMediaQuerySet();
        $$->addMediaQuery(parser->sinkFloatingMediaQuery($1));
    }
    | mq_list media_query ',' maybe_space location_label {
        $$ = $1;
        $$->addMediaQuery(parser->sinkFloatingMediaQuery($2));
    }
    ;

at_rule_body_start:
    /* empty */ {
        parser->startRuleBody();
    }
    ;

before_media_rule:
    /* empty */ {
        parser->startRuleHeader(CSSRuleSourceData::MEDIA_RULE);
    }
    ;

at_rule_header_end_maybe_space:
    maybe_space {
        parser->endRuleHeader();
    }
    ;

media_rule_start:
    before_media_rule MEDIA_SYM maybe_space;

media:
    media_rule_start maybe_media_list at_rule_header_end '{' at_rule_body_start maybe_space block_rule_body closing_brace {
        $$ = parser->createMediaRule($2, $7);
    }
    ;

medium:
  IDENT
  ;

supports:
    before_supports_rule SUPPORTS_SYM maybe_space supports_condition at_supports_rule_header_end '{' at_rule_body_start maybe_space block_rule_body closing_brace {
        $$ = parser->createSupportsRule($4, $9);
    }
    ;

before_supports_rule:
    /* empty */ {
        parser->startRuleHeader(CSSRuleSourceData::SUPPORTS_RULE);
        parser->markSupportsRuleHeaderStart();
    }
    ;

at_supports_rule_header_end:
    /* empty */ {
        parser->endRuleHeader();
        parser->markSupportsRuleHeaderEnd();
    }
    ;

supports_condition:
    supports_condition_in_parens
    | supports_negation
    | supports_conjunction
    | supports_disjunction
    ;

supports_negation:
    SUPPORTS_NOT maybe_space supports_condition_in_parens {
        $$ = !$3;
    }
    ;

supports_conjunction:
    supports_condition_in_parens SUPPORTS_AND maybe_space supports_condition_in_parens {
        $$ = $1 && $4;
    }
    | supports_conjunction SUPPORTS_AND maybe_space supports_condition_in_parens {
        $$ = $1 && $4;
    }
    ;

supports_disjunction:
    supports_condition_in_parens SUPPORTS_OR maybe_space supports_condition_in_parens {
        $$ = $1 || $4;
    }
    | supports_disjunction SUPPORTS_OR maybe_space supports_condition_in_parens {
        $$ = $1 || $4;
    }
    ;

supports_condition_in_parens:
    '(' maybe_space supports_condition closing_parenthesis maybe_space {
        $$ = $3;
    }
    | supports_declaration_condition
    | '(' error error_location error_recovery closing_parenthesis maybe_space {
        parser->reportError($3, InvalidSupportsConditionCSSError);
        $$ = false;
    }
    ;

supports_declaration_condition:
    '(' maybe_space IDENT maybe_space ':' maybe_space expr prio closing_parenthesis maybe_space {
        $$ = false;
        CSSPropertyID id = cssPropertyID($3);
        if (id != CSSPropertyInvalid) {
            parser->m_valueList = parser->sinkFloatingValueList($7);
            int oldParsedProperties = parser->m_parsedProperties.size();
            $$ = parser->parseValue(id, $8);
            // We just need to know if the declaration is supported as it is written. Rollback any additions.
            if ($$)
                parser->rollbackLastProperties(parser->m_parsedProperties.size() - oldParsedProperties);
        }
        parser->m_valueList = nullptr;
        parser->endProperty($8, false);
    }
    | '(' maybe_space IDENT maybe_space ':' maybe_space error error_recovery closing_parenthesis maybe_space {
        $$ = false;
        parser->endProperty(false, false, GeneralCSSError);
    }
    ;

before_keyframes_rule:
    /* empty */ {
        parser->startRuleHeader(CSSRuleSourceData::KEYFRAMES_RULE);
    }
    ;

keyframes_rule_start:
    before_keyframes_rule KEYFRAMES_SYM maybe_space {
        $$ = false;
    }
  | before_keyframes_rule WEBKIT_KEYFRAMES_SYM maybe_space {
        $$ = true;
    }
    ;

keyframes:
    keyframes_rule_start keyframe_name at_rule_header_end_maybe_space '{' at_rule_body_start maybe_space location_label keyframes_rule closing_brace {
        $$ = parser->createKeyframesRule($2, parser->sinkFloatingKeyframeVector($8), $1 /* isPrefixed */);
    }
    ;

keyframe_name:
    IDENT
    | STRING
    ;

keyframes_rule:
    keyframe_rule_list
    | keyframe_rule_list keyframes_error_recovery {
        parser->clearProperties();
    };

keyframe_rule_list:
    /* empty */ {
        $$ = parser->createFloatingKeyframeVector();
        parser->resumeErrorLogging();
    }
    |  keyframe_rule_list keyframe_rule maybe_space location_label {
        $$ = $1;
        $$->append($2);
    }
    | keyframe_rule_list keyframes_error_recovery invalid_block maybe_space location_label {
        parser->clearProperties();
        parser->resumeErrorLogging();
    }
    ;

keyframe_rule:
    key_list '{' maybe_space declaration_list closing_brace {
        $$ = parser->createKeyframe($1);
    }
    ;

key_list:
    key maybe_space {
        $$ = parser->createFloatingValueList();
        $$->addValue(parser->sinkFloatingValue($1));
    }
    | key_list ',' maybe_space key maybe_space {
        $$ = $1;
        $$->addValue(parser->sinkFloatingValue($4));
    }
    ;

key:
    maybe_unary_operator PERCENTAGE {
        $$.setFromNumber($1 * $2);
    }
    | IDENT {
        if ($1.equalIgnoringCase("from"))
            $$.setFromNumber(0);
        else if ($1.equalIgnoringCase("to"))
            $$.setFromNumber(100);
        else {
            YYERROR;
        }
    }
    ;

keyframes_error_recovery:
    error rule_error_recovery {
        parser->reportError(parser->lastLocationLabel(), InvalidKeyframeSelectorCSSError);
    }
    ;

before_page_rule:
    /* empty */ {
        parser->startRuleHeader(CSSRuleSourceData::PAGE_RULE);
    }
    ;

page:
    before_page_rule PAGE_SYM maybe_space page_selector at_rule_header_end
    '{' at_rule_body_start maybe_space_before_declaration declarations_and_margins closing_brace {
        if ($4)
            $$ = parser->createPageRule(parser->sinkFloatingSelector($4));
        else {
            // Clear properties in the invalid @page rule.
            parser->clearProperties();
            // Also clear margin at-rules here once we fully implement margin at-rules parsing.
            $$ = 0;
        }
    }
    ;

page_selector:
    IDENT maybe_space {
        $$ = parser->createFloatingSelectorWithTagName(QualifiedName(nullAtom, $1, parser->m_defaultNamespace));
        $$->setForPage();
    }
    | IDENT pseudo_page maybe_space {
        $$ = $2;
        $$->prependTagSelector(QualifiedName(nullAtom, $1, parser->m_defaultNamespace));
        $$->setForPage();
    }
    | pseudo_page maybe_space {
        $$ = $1;
        $$->setForPage();
    }
    | /* empty */ {
        $$ = parser->createFloatingSelector();
        $$->setForPage();
    }
    ;

declarations_and_margins:
    declaration_list
    | declarations_and_margins margin_box maybe_space declaration_list
    ;

margin_box:
    margin_sym {
        parser->startDeclarationsForMarginBox();
    } maybe_space '{' maybe_space declaration_list closing_brace {
        $$ = parser->createMarginAtRule($1);
    }
    ;

margin_sym :
    TOPLEFTCORNER_SYM {
        $$ = CSSSelector::TopLeftCornerMarginBox;
    }
    | TOPLEFT_SYM {
        $$ = CSSSelector::TopLeftMarginBox;
    }
    | TOPCENTER_SYM {
        $$ = CSSSelector::TopCenterMarginBox;
    }
    | TOPRIGHT_SYM {
        $$ = CSSSelector::TopRightMarginBox;
    }
    | TOPRIGHTCORNER_SYM {
        $$ = CSSSelector::TopRightCornerMarginBox;
    }
    | BOTTOMLEFTCORNER_SYM {
        $$ = CSSSelector::BottomLeftCornerMarginBox;
    }
    | BOTTOMLEFT_SYM {
        $$ = CSSSelector::BottomLeftMarginBox;
    }
    | BOTTOMCENTER_SYM {
        $$ = CSSSelector::BottomCenterMarginBox;
    }
    | BOTTOMRIGHT_SYM {
        $$ = CSSSelector::BottomRightMarginBox;
    }
    | BOTTOMRIGHTCORNER_SYM {
        $$ = CSSSelector::BottomRightCornerMarginBox;
    }
    | LEFTTOP_SYM {
        $$ = CSSSelector::LeftTopMarginBox;
    }
    | LEFTMIDDLE_SYM {
        $$ = CSSSelector::LeftMiddleMarginBox;
    }
    | LEFTBOTTOM_SYM {
        $$ = CSSSelector::LeftBottomMarginBox;
    }
    | RIGHTTOP_SYM {
        $$ = CSSSelector::RightTopMarginBox;
    }
    | RIGHTMIDDLE_SYM {
        $$ = CSSSelector::RightMiddleMarginBox;
    }
    | RIGHTBOTTOM_SYM {
        $$ = CSSSelector::RightBottomMarginBox;
    }
    ;

before_font_face_rule:
    /* empty */ {
        parser->startRuleHeader(CSSRuleSourceData::FONT_FACE_RULE);
    }
    ;

font_face:
    before_font_face_rule FONT_FACE_SYM at_rule_header_end_maybe_space
    '{' at_rule_body_start maybe_space_before_declaration declaration_list closing_brace {
        $$ = parser->createFontFaceRule();
    }
    ;

before_viewport_rule:
    /* empty */ {
        parser->markViewportRuleBodyStart();
        parser->startRuleHeader(CSSRuleSourceData::VIEWPORT_RULE);
    }
    ;

viewport:
    before_viewport_rule VIEWPORT_RULE_SYM at_rule_header_end_maybe_space
    '{' at_rule_body_start maybe_space_before_declaration declaration_list closing_brace {
        $$ = parser->createViewportRule();
        parser->markViewportRuleBodyEnd();
    }
;

combinator:
    '+' maybe_space { $$ = CSSSelector::DirectAdjacent; }
    | '~' maybe_space { $$ = CSSSelector::IndirectAdjacent; }
    | '>' maybe_space { $$ = CSSSelector::Child; }
    // FIXME: implement named combinator and replace the following /shadow/, /shadow-child/ and
    // /shadow-deep/ with named combinator's implementation.
    | '/' IDENT '/' maybe_space {
        if (!RuntimeEnabledFeatures::shadowDOMEnabled())
            YYERROR;
        if ($2.equalIgnoringCase("shadow"))
            $$ = CSSSelector::Shadow;
        else if ($2.equalIgnoringCase("shadow-deep"))
            $$ = CSSSelector::ShadowDeep;
        else if ($2.equalIgnoringCase("content"))
            $$ = CSSSelector::ShadowContent;
        else
            YYERROR;
    }
    ;

maybe_unary_operator:
    unary_operator
    | /* empty */ { $$ = 1; }
    ;

unary_operator:
    '-' { $$ = -1; }
  | '+' { $$ = 1; }
  ;

maybe_space_before_declaration:
    maybe_space {
        parser->startProperty();
    }
  ;

before_selector_list:
    /* empty */ {
        parser->startRuleHeader(CSSRuleSourceData::STYLE_RULE);
        parser->startSelector();
    }
  ;

at_rule_header_end:
    /* empty */ {
        parser->endRuleHeader();
    }
  ;

at_selector_end:
    /* empty */ {
        parser->endSelector();
    }
  ;

ruleset:
    before_selector_list selector_list at_selector_end at_rule_header_end '{' at_rule_body_start maybe_space_before_declaration declaration_list closing_brace {
        $$ = parser->createStyleRule($2);
    }
  ;

before_selector_group_item:
    /* empty */ {
        parser->startSelector();
    }

selector_list:
    selector %prec UNIMPORTANT_TOK {
        $$ = parser->reusableSelectorVector();
        $$->shrink(0);
        $$->append(parser->sinkFloatingSelector($1));
    }
    | selector_list at_selector_end ',' maybe_space before_selector_group_item selector %prec UNIMPORTANT_TOK {
        $$ = $1;
        $$->append(parser->sinkFloatingSelector($6));
    }
   ;

relative_selector:
    combinator selector {
        $$ = $2;
        CSSParserSelector* end = $$;
        while (end->tagHistory())
            end = end->tagHistory();
        end->setRelation($1);
    }
    | selector
    ;

selector:
    simple_selector
    | selector WHITESPACE
    | selector WHITESPACE simple_selector
    {
        $$ = $3;
        CSSParserSelector* end = $$;
        while (end->tagHistory())
            end = end->tagHistory();
        end->setRelation(CSSSelector::Descendant);
        end->setTagHistory(parser->sinkFloatingSelector($1));
    }
    | selector combinator simple_selector {
        $$ = $3;
        CSSParserSelector* end = $$;
        while (end->tagHistory())
            end = end->tagHistory();
        end->setRelation($2);
        end->setTagHistory(parser->sinkFloatingSelector($1));
    }
    ;

namespace_selector:
    /* empty */ '|' { $$.clear(); }
    | '*' '|' { static const LChar star = '*'; $$.init(&star, 1); }
    | IDENT '|'
    ;

simple_selector:
    element_name {
        $$ = parser->createFloatingSelectorWithTagName(QualifiedName(nullAtom, $1, parser->m_defaultNamespace));
    }
    | element_name specifier_list {
        $$ = parser->rewriteSpecifiersWithElementName(nullAtom, $1, $2);
        if (!$$)
            YYERROR;
    }
    | specifier_list {
        $$ = parser->rewriteSpecifiersWithNamespaceIfNeeded($1);
        if (!$$)
            YYERROR;
    }
    | namespace_selector element_name {
        $$ = parser->createFloatingSelectorWithTagName(parser->determineNameInNamespace($1, $2));
        if (!$$)
            YYERROR;
    }
    | namespace_selector element_name specifier_list {
        $$ = parser->rewriteSpecifiersWithElementName($1, $2, $3);
        if (!$$)
            YYERROR;
    }
    | namespace_selector specifier_list {
        $$ = parser->rewriteSpecifiersWithElementName($1, starAtom, $2);
        if (!$$)
            YYERROR;
    }
  ;

simple_selector_list:
    simple_selector %prec UNIMPORTANT_TOK {
        $$ = parser->createFloatingSelectorVector();
        $$->append(parser->sinkFloatingSelector($1));
    }
    | simple_selector_list maybe_space ',' maybe_space simple_selector %prec UNIMPORTANT_TOK {
        $$ = $1;
        $$->append(parser->sinkFloatingSelector($5));
    }
  ;

element_name:
    IDENT {
        if (parser->m_context.isHTMLDocument())
            parser->tokenToLowerCase($1);
        $$ = $1;
    }
    | '*' {
        static const LChar star = '*';
        $$.init(&star, 1);
    }
  ;

specifier_list:
    specifier
    | specifier_list specifier {
        $$ = parser->rewriteSpecifiers($1, $2);
    }
;

specifier:
    IDSEL {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::Id);
        if (isQuirksModeBehavior(parser->m_context.mode()))
            parser->tokenToLowerCase($1);
        $$->setValue($1);
    }
  | HEX {
        if ($1[0] >= '0' && $1[0] <= '9') {
            YYERROR;
        } else {
            $$ = parser->createFloatingSelector();
            $$->setMatch(CSSSelector::Id);
            if (isQuirksModeBehavior(parser->m_context.mode()))
                parser->tokenToLowerCase($1);
            $$->setValue($1);
        }
    }
  | class
  | attrib
  | pseudo
    ;

class:
    '.' IDENT {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::Class);
        if (isQuirksModeBehavior(parser->m_context.mode()))
            parser->tokenToLowerCase($2);
        $$->setValue($2);
    }
  ;

attr_name:
    IDENT maybe_space {
        if (parser->m_context.isHTMLDocument())
            parser->tokenToLowerCase($1);
        $$ = $1;
    }
    ;

attrib:
    '[' maybe_space attr_name closing_square_bracket {
        $$ = parser->createFloatingSelector();
        $$->setAttribute(QualifiedName(nullAtom, $3, nullAtom));
        $$->setMatch(CSSSelector::Set);
    }
    | '[' maybe_space attr_name match maybe_space ident_or_string maybe_space closing_square_bracket {
        $$ = parser->createFloatingSelector();
        $$->setAttribute(QualifiedName(nullAtom, $3, nullAtom));
        $$->setMatch((CSSSelector::Match)$4);
        $$->setValue($6);
    }
    | '[' maybe_space namespace_selector attr_name closing_square_bracket {
        $$ = parser->createFloatingSelector();
        $$->setAttribute(parser->determineNameInNamespace($3, $4));
        $$->setMatch(CSSSelector::Set);
    }
    | '[' maybe_space namespace_selector attr_name match maybe_space ident_or_string maybe_space closing_square_bracket {
        $$ = parser->createFloatingSelector();
        $$->setAttribute(parser->determineNameInNamespace($3, $4));
        $$->setMatch((CSSSelector::Match)$5);
        $$->setValue($7);
    }
    | '[' selector_recovery closing_square_bracket {
        YYERROR;
    }
  ;

match:
    '=' {
        $$ = CSSSelector::Exact;
    }
    | INCLUDES {
        $$ = CSSSelector::List;
    }
    | DASHMATCH {
        $$ = CSSSelector::Hyphen;
    }
    | BEGINSWITH {
        $$ = CSSSelector::Begin;
    }
    | ENDSWITH {
        $$ = CSSSelector::End;
    }
    | CONTAINS {
        $$ = CSSSelector::Contain;
    }
    ;

ident_or_string:
    IDENT
  | STRING
    ;

pseudo_page:
    ':' IDENT {
        if ($2.isFunction())
            YYERROR;
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PagePseudoClass);
        parser->tokenToLowerCase($2);
        $$->setValue($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            YYERROR;
    }

pseudo:
    ':' error_location IDENT {
        if ($3.isFunction())
            YYERROR;
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        parser->tokenToLowerCase($3);
        $$->setValue($3);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown) {
            parser->reportError($2, InvalidSelectorPseudoCSSError);
            YYERROR;
        }
    }
    | ':' ':' error_location IDENT {
        if ($4.isFunction())
            YYERROR;
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoElement);
        parser->tokenToLowerCase($4);
        $$->setValue($4);
        // FIXME: This call is needed to force selector to compute the pseudoType early enough.
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown) {
            parser->reportError($3, InvalidSelectorPseudoCSSError);
            YYERROR;
        }
    }
    // used by ::cue(:past/:future)
    | ':' ':' CUEFUNCTION maybe_space simple_selector_list maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoElement);
        $$->adoptSelectorVector(*parser->sinkFloatingSelectorVector($5));
        $$->setValue($3);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type != CSSSelector::PseudoCue)
            YYERROR;
    }
    | ':' ':' CUEFUNCTION selector_recovery closing_parenthesis {
        YYERROR;
    }
    | ':' ':' DISTRIBUTEDFUNCTION maybe_space relative_selector closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoElement);
        $$->setFunctionArgumentSelector($5);
        parser->tokenToLowerCase($3);
        $$->setValue($3);
    }
    | ':' ':' DISTRIBUTEDFUNCTION selector_recovery closing_parenthesis {
        YYERROR;
    }
    // use by :-webkit-any.
    // FIXME: should we support generic selectors here or just simple_selectors?
    // Use simple_selector_list for now to match -moz-any.
    // See http://lists.w3.org/Archives/Public/www-style/2010Sep/0566.html for some
    // related discussion with respect to :not.
    | ':' ANYFUNCTION maybe_space simple_selector_list maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        $$->adoptSelectorVector(*parser->sinkFloatingSelectorVector($4));
        parser->tokenToLowerCase($2);
        $$->setValue($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type != CSSSelector::PseudoAny)
            YYERROR;
    }
    | ':' ANYFUNCTION selector_recovery closing_parenthesis {
        YYERROR;
    }
    // used by :nth-*(ax+b)
    | ':' FUNCTION maybe_space NTH maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        $$->setArgument($4);
        $$->setValue($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            YYERROR;
    }
    // used by :nth-*
    | ':' FUNCTION maybe_space maybe_unary_operator INTEGER maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        $$->setArgument(AtomicString::number($4 * $5));
        $$->setValue($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            YYERROR;
    }
    // used by :nth-*(odd/even) and :lang
    | ':' FUNCTION maybe_space IDENT maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        $$->setArgument($4);
        parser->tokenToLowerCase($2);
        $$->setValue($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            YYERROR;
        else if (type == CSSSelector::PseudoNthChild ||
                 type == CSSSelector::PseudoNthOfType ||
                 type == CSSSelector::PseudoNthLastChild ||
                 type == CSSSelector::PseudoNthLastOfType) {
            if (!isValidNthToken($4))
                YYERROR;
        }
    }
    | ':' FUNCTION selector_recovery closing_parenthesis {
        YYERROR;
    }
    // used by :not
    | ':' NOTFUNCTION maybe_space simple_selector maybe_space closing_parenthesis {
        if (!$4->isSimple())
            YYERROR;
        else {
            $$ = parser->createFloatingSelector();
            $$->setMatch(CSSSelector::PseudoClass);

            Vector<OwnPtr<CSSParserSelector> > selectorVector;
            selectorVector.append(parser->sinkFloatingSelector($4));
            $$->adoptSelectorVector(selectorVector);

            parser->tokenToLowerCase($2);
            $$->setValue($2);
        }
    }
    | ':' NOTFUNCTION selector_recovery closing_parenthesis {
        YYERROR;
    }
    | ':' HOSTFUNCTION maybe_space simple_selector_list maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        $$->adoptSelectorVector(*parser->sinkFloatingSelectorVector($4));
        parser->tokenToLowerCase($2);
        $$->setValue($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type != CSSSelector::PseudoHost)
            YYERROR;
    }
    //  used by :host()
    | ':' HOSTFUNCTION maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        parser->tokenToLowerCase($2);
        $$->setValue($2.atomicSubstring(0, $2.length() - 1));
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type != CSSSelector::PseudoHost)
            YYERROR;
    }
    | ':' HOSTFUNCTION selector_recovery closing_parenthesis {
        YYERROR;
    }
    | ':' ANCESTORFUNCTION maybe_space simple_selector_list maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        $$->adoptSelectorVector(*parser->sinkFloatingSelectorVector($4));
        parser->tokenToLowerCase($2);
        $$->setValue($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type != CSSSelector::PseudoAncestor)
            YYERROR;
    }
    //  used by :ancestor()
    | ':' ANCESTORFUNCTION maybe_space closing_parenthesis {
        $$ = parser->createFloatingSelector();
        $$->setMatch(CSSSelector::PseudoClass);
        parser->tokenToLowerCase($2);
        $$->setValue($2.atomicSubstring(0, $2.length() - 1));
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type != CSSSelector::PseudoAncestor)
            YYERROR;
    }
    | ':' ANCESTORFUNCTION selector_recovery closing_parenthesis {
        YYERROR;
    }
  ;

selector_recovery:
    error error_location error_recovery;

declaration_list:
    /* empty */ { $$ = false; }
    | declaration
    | decl_list declaration {
        $$ = $1 || $2;
    }
    | decl_list
    ;

decl_list:
    declaration ';' maybe_space {
        parser->startProperty();
        $$ = $1;
    }
    | decl_list declaration ';' maybe_space {
        parser->startProperty();
        $$ = $1 || $2;
    }
    ;

declaration:
    property ':' maybe_space error_location expr prio {
        $$ = false;
        bool isPropertyParsed = false;
        if ($1 != CSSPropertyInvalid) {
            parser->m_valueList = parser->sinkFloatingValueList($5);
            int oldParsedProperties = parser->m_parsedProperties.size();
            $$ = parser->parseValue($1, $6);
            if (!$$) {
                parser->rollbackLastProperties(parser->m_parsedProperties.size() - oldParsedProperties);
                parser->reportError($4, InvalidPropertyValueCSSError);
            } else
                isPropertyParsed = true;
            parser->m_valueList = nullptr;
        }
        parser->endProperty($6, isPropertyParsed);
    }
    |
    property ':' maybe_space error_location expr prio error error_recovery {
        /* When we encounter something like p {color: red !important fail;} we should drop the declaration */
        parser->reportError($4, InvalidPropertyValueCSSError);
        parser->endProperty(false, false);
        $$ = false;
    }
    |
    property ':' maybe_space error_location error error_recovery {
        parser->reportError($4, InvalidPropertyValueCSSError);
        parser->endProperty(false, false);
        $$ = false;
    }
    |
    property error error_location error_recovery {
        parser->reportError($3, PropertyDeclarationCSSError);
        parser->endProperty(false, false, GeneralCSSError);
        $$ = false;
    }
    |
    error error_location error_recovery {
        parser->reportError($2, PropertyDeclarationCSSError);
        $$ = false;
    }
  ;

property:
    error_location IDENT maybe_space {
        $$ = cssPropertyID($2);
        parser->setCurrentProperty($$);
        if ($$ == CSSPropertyInvalid)
            parser->reportError($1, InvalidPropertyCSSError);
    }
  ;

prio:
    IMPORTANT_SYM maybe_space { $$ = true; }
    | /* empty */ { $$ = false; }
  ;

ident_list:
    IDENT maybe_space {
        $$ = parser->createFloatingValueList();
        $$->addValue(makeIdentValue($1));
    }
    | ident_list IDENT maybe_space {
        $$ = $1;
        $$->addValue(makeIdentValue($2));
    }
    ;

track_names_list:
    '(' maybe_space closing_parenthesis {
        $$.setFromValueList(parser->sinkFloatingValueList(parser->createFloatingValueList()));
    }
    | '(' maybe_space ident_list closing_parenthesis {
        $$.setFromValueList(parser->sinkFloatingValueList($3));
    }
    | '(' maybe_space expr_recovery closing_parenthesis {
        YYERROR;
    }
  ;

expr:
    term {
        $$ = parser->createFloatingValueList();
        $$->addValue(parser->sinkFloatingValue($1));
    }
    | expr operator term {
        $$ = $1;
        $$->addValue(makeOperatorValue($2));
        $$->addValue(parser->sinkFloatingValue($3));
    }
    | expr term {
        $$ = $1;
        $$->addValue(parser->sinkFloatingValue($2));
    }
  ;

expr_recovery:
    error error_location error_recovery {
        parser->reportError($2, PropertyDeclarationCSSError);
    }
  ;

operator:
    '/' maybe_space {
        $$ = '/';
    }
  | ',' maybe_space {
        $$ = ',';
    }
  ;

term:
  unary_term maybe_space
  | unary_operator unary_term maybe_space { $$ = $2; $$.fValue *= $1; }
  | STRING maybe_space { $$.id = CSSValueInvalid; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_STRING; }
  | IDENT maybe_space { $$ = makeIdentValue($1); }
  /* We might need to actually parse the number from a dimension, but we can't just put something that uses $$.string into unary_term. */
  | DIMEN maybe_space { $$.id = CSSValueInvalid; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_DIMENSION; }
  | unary_operator DIMEN maybe_space { $$.id = CSSValueInvalid; $$.string = $2; $$.unit = CSSPrimitiveValue::CSS_DIMENSION; }
  | URI maybe_space { $$.id = CSSValueInvalid; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_URI; }
  | UNICODERANGE maybe_space { $$.id = CSSValueInvalid; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_UNICODE_RANGE; }
  | HEX maybe_space { $$.id = CSSValueInvalid; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_PARSER_HEXCOLOR; }
  | '#' maybe_space { $$.id = CSSValueInvalid; $$.string = CSSParserString(); $$.unit = CSSPrimitiveValue::CSS_PARSER_HEXCOLOR; } /* Handle error case: "color: #;" */
  /* FIXME: according to the specs a function can have a unary_operator in front. I know no case where this makes sense */
  | function maybe_space
  | calc_function maybe_space
  | min_or_max_function maybe_space
  | '%' maybe_space { /* Handle width: %; */
      $$.id = CSSValueInvalid; $$.unit = 0;
  }
  | track_names_list maybe_space
  ;

unary_term:
  INTEGER { $$.setFromNumber($1); $$.isInt = true; }
  | FLOATTOKEN { $$.setFromNumber($1); }
  | PERCENTAGE { $$.setFromNumber($1, CSSPrimitiveValue::CSS_PERCENTAGE); }
  | PXS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_PX); }
  | CMS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_CM); }
  | MMS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_MM); }
  | INS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_IN); }
  | PTS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_PT); }
  | PCS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_PC); }
  | DEGS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_DEG); }
  | RADS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_RAD); }
  | GRADS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_GRAD); }
  | TURNS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_TURN); }
  | MSECS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_MS); }
  | SECS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_S); }
  | HERTZ { $$.setFromNumber($1, CSSPrimitiveValue::CSS_HZ); }
  | KHERTZ { $$.setFromNumber($1, CSSPrimitiveValue::CSS_KHZ); }
  | EMS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_EMS); }
  | QEMS { $$.setFromNumber($1, CSSParserValue::Q_EMS); }
  | EXS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_EXS); }
  | REMS {
      $$.setFromNumber($1, CSSPrimitiveValue::CSS_REMS);
      if (parser->m_styleSheet)
          parser->m_styleSheet->parserSetUsesRemUnits(true);
  }
  | CHS { $$.setFromNumber($1, CSSPrimitiveValue::CSS_CHS); }
  | VW { $$.setFromNumber($1, CSSPrimitiveValue::CSS_VW); }
  | VH { $$.setFromNumber($1, CSSPrimitiveValue::CSS_VH); }
  | VMIN { $$.setFromNumber($1, CSSPrimitiveValue::CSS_VMIN); }
  | VMAX { $$.setFromNumber($1, CSSPrimitiveValue::CSS_VMAX); }
  | DPPX { $$.setFromNumber($1, CSSPrimitiveValue::CSS_DPPX); }
  | DPI { $$.setFromNumber($1, CSSPrimitiveValue::CSS_DPI); }
  | DPCM { $$.setFromNumber($1, CSSPrimitiveValue::CSS_DPCM); }
  | FR { $$.setFromNumber($1, CSSPrimitiveValue::CSS_FR); }
  ;

function:
    FUNCTION maybe_space expr closing_parenthesis {
        $$.setFromFunction(parser->createFloatingFunction($1, parser->sinkFloatingValueList($3)));
    } |
    FUNCTION maybe_space closing_parenthesis {
        $$.setFromFunction(parser->createFloatingFunction($1, parser->sinkFloatingValueList(parser->createFloatingValueList())));
    } |
    FUNCTION maybe_space expr_recovery closing_parenthesis {
        YYERROR;
    }
  ;

calc_func_term:
  unary_term
  | unary_operator unary_term { $$ = $2; $$.fValue *= $1; }
  ;

calc_func_operator:
    space '+' space {
        $$ = '+';
    }
    | space '-' space {
        $$ = '-';
    }
    | calc_maybe_space '*' maybe_space {
        $$ = '*';
    }
    | calc_maybe_space '/' maybe_space {
        $$ = '/';
    }
  ;

calc_maybe_space:
    /* empty */
    | WHITESPACE
  ;

calc_func_paren_expr:
    '(' maybe_space calc_func_expr calc_maybe_space closing_parenthesis {
        $$ = $3;
        $$->insertValueAt(0, makeOperatorValue('('));
        $$->addValue(makeOperatorValue(')'));
    }
    | '(' maybe_space expr_recovery closing_parenthesis {
        YYERROR;
    }
  ;

calc_func_expr:
    calc_func_term {
        $$ = parser->createFloatingValueList();
        $$->addValue(parser->sinkFloatingValue($1));
    }
    | calc_func_expr calc_func_operator calc_func_term {
        $$ = $1;
        $$->addValue(makeOperatorValue($2));
        $$->addValue(parser->sinkFloatingValue($3));
    }
    | calc_func_expr calc_func_operator calc_func_paren_expr {
        $$ = $1;
        $$->addValue(makeOperatorValue($2));
        $$->stealValues(*($3));
    }
    | calc_func_paren_expr
  ;

calc_func_expr_list:
    calc_func_expr calc_maybe_space
    | calc_func_expr_list ',' maybe_space calc_func_expr calc_maybe_space {
        $$ = $1;
        $$->addValue(makeOperatorValue(','));
        $$->stealValues(*($4));
    }
  ;

calc_function:
    CALCFUNCTION maybe_space calc_func_expr calc_maybe_space closing_parenthesis {
        $$.setFromFunction(parser->createFloatingFunction($1, parser->sinkFloatingValueList($3)));
    }
    | CALCFUNCTION maybe_space expr_recovery closing_parenthesis {
        YYERROR;
    }
    ;


min_or_max:
    MINFUNCTION
    | MAXFUNCTION
    ;

min_or_max_function:
    min_or_max maybe_space calc_func_expr_list closing_parenthesis {
        $$.setFromFunction(parser->createFloatingFunction($1, parser->sinkFloatingValueList($3)));
    }
    | min_or_max maybe_space expr_recovery closing_parenthesis {
        YYERROR;
    }
    ;

invalid_at:
    ATKEYWORD
  | margin_sym
    ;

at_rule_recovery:
    at_rule_header_recovery at_invalid_rule_header_end at_rule_end
    ;

at_rule_header_recovery:
    error error_location rule_error_recovery {
        parser->reportError($2, InvalidRuleCSSError);
    }
    ;

at_rule_end:
    at_invalid_rule_header_end semi_or_eof
  | at_invalid_rule_header_end invalid_block
    ;

regular_invalid_at_rule_header:
    keyframes_rule_start at_rule_header_recovery
  | before_page_rule PAGE_SYM at_rule_header_recovery
  | before_font_face_rule FONT_FACE_SYM at_rule_header_recovery
  | before_supports_rule SUPPORTS_SYM error error_location rule_error_recovery {
        parser->reportError($4, InvalidSupportsConditionCSSError);
        parser->popSupportsRuleData();
    }
  | before_viewport_rule VIEWPORT_RULE_SYM at_rule_header_recovery {
        parser->markViewportRuleBodyEnd();
    }
  | import_rule_start at_rule_header_recovery
  | NAMESPACE_SYM at_rule_header_recovery
  | error_location invalid_at at_rule_header_recovery {
        parser->resumeErrorLogging();
        parser->reportError($1, InvalidRuleCSSError);
    }
  ;

invalid_rule:
    error error_location rule_error_recovery at_invalid_rule_header_end invalid_block {
        parser->reportError($2, InvalidRuleCSSError);
    }
  | regular_invalid_at_rule_header at_invalid_rule_header_end ';'
  | regular_invalid_at_rule_header at_invalid_rule_header_end invalid_block
  | media_rule_start maybe_media_list ';'
    ;

invalid_rule_header:
    error error_location rule_error_recovery at_invalid_rule_header_end {
        parser->reportError($2, InvalidRuleCSSError);
    }
  | regular_invalid_at_rule_header at_invalid_rule_header_end
  | media_rule_start maybe_media_list
    ;

at_invalid_rule_header_end:
   /* empty */ {
       parser->endInvalidRuleHeader();
   }
   ;

invalid_block:
    '{' error_recovery closing_brace {
        parser->invalidBlockHit();
    }
    ;

invalid_square_brackets_block:
    '[' error_recovery closing_square_bracket
    ;

invalid_parentheses_block:
    opening_parenthesis error_recovery closing_parenthesis;

opening_parenthesis:
    '(' | FUNCTION | CALCFUNCTION | MINFUNCTION | MAXFUNCTION | ANYFUNCTION | NOTFUNCTION | CUEFUNCTION | DISTRIBUTEDFUNCTION | HOSTFUNCTION
    ;

error_location: {
        $$ = parser->currentLocation();
    }
    ;

location_label: {
        parser->setLocationLabel(parser->currentLocation());
    }
    ;

error_recovery:
    /* empty */
  | error_recovery error
  | error_recovery invalid_block
  | error_recovery invalid_square_brackets_block
  | error_recovery invalid_parentheses_block
    ;

rule_error_recovery:
    /* empty */
  | rule_error_recovery error
  | rule_error_recovery invalid_square_brackets_block
  | rule_error_recovery invalid_parentheses_block
    ;

%%

