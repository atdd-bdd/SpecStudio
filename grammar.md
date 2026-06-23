File          ::= { TopLevelCommand | CommentLine } ;

TopLevelCommand ::=
      Specification
    | DataType
    | DomainTerm
    | BusinessRule
    | Calculation
    | Entity
    | AttributesBlock
    | Scenario
    | ScenarioGroup
    | Background
    | ImportDirective
    | InsertDirective ;

CommentLine   ::= "#" { ANY_CHAR } ;

NamedComment  ::= DescriptionBlock | DetailsBlock | ConstraintBlock ;

DescriptionBlock ::= "Description" WS TextLine ;
DetailsBlock      ::= "Details" WS "\" NL { IndentedTextLine "\" NL } ;
ConstraintBlock   ::= "Constraint" WS TextLine ;

Specification ::= "Specification" WS TextLine NL
                  { NamedComment NL } ;

DataType ::= "DataType" WS Identifier NL
            { NamedComment NL }
            ( EnumBody | DataTypeBody ) ;

EnumBody ::= TableHeaderRow NL { TableDataRow NL } ;
DataTypeBody ::= "Examples" NL
                 TableHeaderRow NL { TableDataRow NL } ;

DomainTerm ::= "DomainTerm" WS Identifier WS ":" WS Identifier NL
              { NamedComment NL } ;

BusinessRule ::= "BusinessRule" WS Identifier WS ":" WS Identifier NL
                { NamedComment NL }
                "Examples" NL
                TableHeaderRow NL { TableDataRow NL } ;

Calculation ::= "Calculation" WS Identifier WS ":" WS Identifier NL
               { NamedComment NL }
               "Examples" NL
               TableHeaderRow NL { TableDataRow NL } ;

Entity ::= "Entity" WS Identifier NL
          { NamedComment NL }
          TableHeaderRow NL { TableDataRow NL } ;

AttributesBlock ::= "Attributes" WS Identifier NL
                   { NamedComment NL }
                   TableHeaderRow NL { TableDataRow NL } ;

Scenario ::= "Scenario" WS TextLine NL
            { NamedComment NL }
            { StepBlock } ;

StepBlock ::= StepCommand WS StepDesc WS ":" WS Identifier NL
             TableHeaderRow NL { TableDataRow NL } ;

StepCommand ::= "Given" | "When" | "Then" | "And" ;
StepDesc    ::= TextLine ;

Background ::= "Background:" NL
              { NamedComment NL }
              { BackgroundStep } ;

BackgroundStep ::= ("Given" | "And") WS StepDesc WS ":" WS Identifier NL
                   TableHeaderRow NL { TableDataRow NL } ;

ScenarioGroup ::= "ScenarioGroup" WS Identifier NL
                 { NamedComment NL }
                 { Scenario } ;

ImportDirective ::= "Import" WS StringLiteral NL
                   { NamedComment NL } ;

InsertDirective ::= "Insert" WS StringLiteral NL
                   { NamedComment NL } ;

TableHeaderRow ::= "|" WS HeaderCell { WS "|" WS HeaderCell } WS "|" ;
TableDataRow   ::= "|" WS DataCell   { WS "|" WS DataCell   } WS "|" ;

HeaderCell ::= TEXT ;
DataCell   ::= TEXT ;

Identifier  ::= TEXT ;        (* no spaces, or a defined identifier rule *)
StringLiteral ::= '"' { ANY_CHAR_EXCEPT_QUOTE } '"' ;

TextLine    ::= { ANY_CHAR_EXCEPT_NL } ;
WS          ::= { " " | "\t" } ;
NL          ::= "\r\n" | "\n" ;
IndentedTextLine ::= WS { ANY_CHAR_EXCEPT_NL } ;
