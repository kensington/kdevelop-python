#include "declarationbuilder.h"

#include <QByteArray>

#include <ktexteditor/smartrange.h>
#include <ktexteditor/smartinterface.h>

#include <definition.h>
#include <symboltable.h>
#include <forwarddeclaration.h>
#include <duchain.h>
#include <duchainlock.h>
#include "parsesession.h"
#include "pythoneditorintegrator.h"
#include "functiondeclaration.h"
#include "classfunctiondeclaration.h"


using namespace KTextEditor;
using namespace KDevelop;
using namespace python;


// DeclarationBuilder::DeclarationBuilder (ParseSession* session, const KUrl &url):DeclarationBuilderBase(session,url)
// {
// }
// DeclarationBuilder::DeclarationBuilder (PythonEditorIntegrator* editor, const KUrl &url):DeclarationBuilderBase(editor,url)
// {
// }

TopDUContext* DeclarationBuilder::buildDeclarations(ast_node *node)
{
  TopDUContext* top = buildContexts(node);

  Q_ASSERT(m_accessPolicyStack.isEmpty());
  Q_ASSERT(m_functionDefinedStack.isEmpty());

  return top;
}
DUContext* DeclarationBuilder::buildSubDeclarations(const KUrl& url, ast_node *node, KDevelop::DUContext* parent) {
  DUContext* top = buildSubContexts(url, node, parent);

  Q_ASSERT(m_accessPolicyStack.isEmpty());
  Q_ASSERT(m_functionDefinedStack.isEmpty());

  return top;
}

ForwardDeclaration * DeclarationBuilder::openForwardDeclaration(std::size_t name, ast_node * range)
{
  return static_cast<ForwardDeclaration*>(openDeclaration(name, range, false, true));
}

Declaration* DeclarationBuilder::openDefinition(std::size_t name, ast_node* rangeNode, bool isFunction)
{
  return openDeclaration(name, rangeNode, isFunction, false, true);
}

template<class DeclarationType>
DeclarationType* DeclarationBuilder::specialDeclaration( KTextEditor::Range* range )
{
    return new DeclarationType(range, currentContext());
}

template<class DeclarationType>
DeclarationType* DeclarationBuilder::specialDeclaration( KTextEditor::Range* range, int scope )
{
    return new DeclarationType(range, (KDevelop::Declaration::Scope)scope, currentContext());
}

Declaration* DeclarationBuilder::openDeclaration(std::size_t name, ast_node* rangeNode, bool isFunction, bool isForward, bool isDefinition)
{
    DUChainWriteLocker lock(DUChain::lock());
    Declaration::Scope scope = Declaration::GlobalScope;
    switch (currentContext()->type()) 
    {
        case DUContext::Class:
        scope = Declaration::ClassScope;
        break;
        case DUContext::Function:
        scope = Declaration::FunctionScope;
        default:
        break;
    }
    Range newRange = m_editor->findRange(rangeNode);
    QualifiedIdentifier id;
//     if (name) {
//         TypeSpecifierAST* typeSpecifier = 0; //Additional type-specifier for example the return-type of a cast operator
//         id = identifierForName(name);
//         if( typeSpecifier && id == QualifiedIdentifier("operator{...cast...}") ) {
//         if( typeSpecifier->kind == AST::Kind_SimpleTypeSpecifier )
//             visitSimpleTypeSpecifier( static_cast<SimpleTypeSpecifierAST*>( typeSpecifier ) );
//         }
//     }
    Identifier lastId;
    if( !id.isEmpty() )
        lastId = id.last();
    Declaration* declaration = 0;
    if (recompiling())
    {
        QMutexLocker lock(m_editor->smart() ? m_editor->smart()->smartMutex() : 0);
        Range translated = newRange;
        if (m_editor->smart())
            translated = m_editor->smart()->translateFromRevision(translated);
        for (; nextDeclaration() < currentContext()->localDeclarations().count(); ++nextDeclaration()) 
        {
            Declaration* dec = currentContext()->localDeclarations().at(nextDeclaration());
            if (dec->textRange().start() > translated.end() && dec->smartRange()) 
                break;
            if (dec->textRange() == translated && dec->scope() == scope &&
                (id.isEmpty() && dec->identifier().toString().isEmpty()) || (!id.isEmpty() && lastId == dec->identifier()) &&
                dec->isDefinition() == isDefinition)
            {
                if (isForward)
                {
                    if (!dynamic_cast<ForwardDeclaration*>(dec))
                        break;
                }
                else if (isFunction) 
                {
                    if (scope == Declaration::ClassScope)
                    {
                        if (!dynamic_cast<ClassFunctionDeclaration*>(dec))
                            break;
                    }
                    else if (!dynamic_cast<AbstractFunctionDeclaration*>(dec)) 
                    {
                        break;
                    }
                }
                else if (scope == Declaration::ClassScope) 
                {
                    if (!dynamic_cast<ClassMemberDeclaration*>(dec))
                        break;
                }
                declaration = dec;
                //If the declaration does not have a smart-range, upgrade it if possible
                /*if( m_editor->smart() && !declaration->smartRange() ) {
                declaration->setTextRange( m_editor->createRange( newRange ) );
                }*/
                if (currentContext()->type() == DUContext::Class) 
                {
                    ClassMemberDeclaration* classDeclaration = static_cast<ClassMemberDeclaration*>(declaration);
                    if (classDeclaration->accessPolicy() != currentAccessPolicy()) 
                    {
                        classDeclaration->setAccessPolicy(currentAccessPolicy());
                    }
                }
                break;
            }
        }
    }
    if (!declaration)
    {
        Range* prior = m_editor->currentRange();
        Range* range = m_editor->createRange(newRange);
        m_editor->exitCurrentRange();
        Q_ASSERT(m_editor->currentRange() == prior);
        if (isForward)
        {
            declaration = new ForwardDeclaration(range, scope, currentContext());
        }
        else if (isFunction) 
        {
            if (scope == Declaration::ClassScope) 
            {
               declaration = specialDeclaration<ClassFunctionDeclaration>( range );
            }
            else
            {
                declaration = specialDeclaration<FunctionDeclaration>(range, scope );
            }
            if (!m_functionDefinedStack.isEmpty())
               declaration->setDeclarationIsDefinition(m_functionDefinedStack.top());
        }
        else if (scope == Declaration::ClassScope) 
        {
            declaration = specialDeclaration<ClassMemberDeclaration>(range );
        }

        if (isDefinition)
        declaration->setDeclarationIsDefinition(true);
        if (currentContext()->type() == DUContext::Class) 
        {
            if(dynamic_cast<ClassMemberDeclaration*>(declaration)) //It may also be a forward-declaration, not based on ClassMemberDeclaration!
                static_cast<ClassMemberDeclaration*>(declaration)->setAccessPolicy(currentAccessPolicy());
        }
        switch (currentContext()->type()) 
        {
        case DUContext::Global:
        case DUContext::Namespace:
        case DUContext::Class:
            SymbolTable::self()->addDeclaration(declaration);
            break;
        default:
            break;
        }
    }
    setEncountered(declaration);
    m_declarationStack.push(declaration);
    return declaration;
}

