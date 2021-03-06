/*****************************************************************************
 * Copyright (c) 2011-2012 Sven Brauch <svenbrauch@googlemail.com>           *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU General Public License as            *
 * published by the Free Software Foundation; either version 2 of            *
 * the License, or (at your option) any later version.                       *
 *                                                                           *           
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *   
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************
 */

#include <QProcess>
#include <QRegExp>
#include <KStandardDirs>
#include <KTextEditor/View>

#include <language/duchain/functiondeclaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/aliasdeclaration.h>
#include <language/duchain/duchainutils.h>
#include <language/util/includeitem.h>
#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include <language/codecompletion/codecompletionitem.h>
#include <language/codecompletion/codecompletionitemgrouper.h>
#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iproject.h>
#include <interfaces/idocumentcontroller.h>
#include <project/projectmodel.h>

#include "parser/astbuilder.h"
#include "duchain/pythoneditorintegrator.h"
#include "duchain/expressionvisitor.h"
#include "duchain/declarationbuilder.h"
#include "duchain/helpers.h"
#include "duchain/types/unsuretype.h"
#include "duchain/navigation/navigationwidget.h"

#include "worker.h"
#include "context.h"
#include "items/keyword.h"
#include "items/importfile.h"
#include "items/functiondeclaration.h"
#include "items/implementfunction.h"
#include "helpers.h"

using namespace KTextEditor;
using namespace KDevelop;

namespace Python {

PythonCodeCompletionContext::ItemTypeHint PythonCodeCompletionContext::itemTypeHint()
{
    return m_itemTypeHint;
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::completionItems(bool& abort, bool fullCompletion)
{
    QList<CompletionTreeItemPointer> resultingItems;
    DUChainReadLocker lock;
    
    kDebug() << "Line: " << m_position.line;
    
    kDebug() << "Completion type:" << m_operation;
    
    if ( m_operation != FunctionCallCompletion ) {
        KeywordItem::Flags f = (KeywordItem::Flags) ( KeywordItem::ForceLineBeginning | KeywordItem::ImportantItem );
        // TODO group those correctly so they appear at the top
        if ( m_position.line == 0 && m_text.startsWith("#") ) {
            resultingItems << CompletionTreeItemPointer(new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "#!/usr/bin/env python\n", f));
            resultingItems << CompletionTreeItemPointer(new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "#!/usr/bin/env python2.7\n", f));
            resultingItems << CompletionTreeItemPointer(new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "#!/usr/bin/env python3\n", f));
        }
        // TODO fixme
        else if ( m_position.line == 1 && m_text.endsWith("#") ) {
            resultingItems << CompletionTreeItemPointer(new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "# -*- Coding:utf-8 -*-\n\n", f));
        }
    }
    
    // Find all calltips recursively
    if ( parentContext() ) {
        resultingItems.append(parentContext()->completionItems(abort, fullCompletion));
    }
    
    if ( m_operation == PythonCodeCompletionContext::NoCompletion ) {
        kDebug() << "no code completion";
    }
    else if ( m_operation == PythonCodeCompletionContext::GeneratorVariableCompletion ) {
        QList<KeywordItem*> completionItems;
        KDevPG::MemoryPool pool;
        AstBuilder* builder = new AstBuilder(&pool);
        CodeAst* tmpAst = builder->parse(KUrl(), m_guessTypeOfExpression);
        if ( tmpAst ) {
            ExpressionVisitor* v = new ExpressionVisitor(m_duContext.data());
            v->m_forceGlobalSearching = true;
            v->m_scanUntilCursor = m_position;
            v->m_reportUnknownNames = true;
            v->visitCode(tmpAst);
            if ( not v->m_unknownNames.isEmpty() ) {
                if ( v->m_unknownNames.size() >= 2 ) {
                    // we only take the first two, and only two. It gets too much items otherwise.
                    QStringList combinations;
                    combinations << v->m_unknownNames.at(0) + ", " + v->m_unknownNames.at(1);
                    combinations << v->m_unknownNames.at(1) + ", " + v->m_unknownNames.at(0);
                    foreach ( const QString& c, combinations ) {
                        completionItems << new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "" + c + " in ");
                    }
                }
                foreach ( const QString& n, v->m_unknownNames ) {
                    completionItems << new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "" + n + " in ");
                }
            }
            else {
                kWarning() << "No unknown names in generator completion; nothing to do.";
            }
            delete v;
        }
        delete builder;
        
        foreach ( KeywordItem* item, completionItems ) {
            resultingItems << CompletionTreeItemPointer(item);
        }
        
    }
    else if ( m_operation == PythonCodeCompletionContext::FunctionCallCompletion ) {
            // gather additional items to show above the real ones (for parameters, and stuff)
            QList<Declaration*> calltips;
            KDevPG::MemoryPool pool;
            AstBuilder* builder = new AstBuilder(&pool);
            CodeAst* tmpAst = builder->parse(KUrl(), m_guessTypeOfExpression);
            if ( tmpAst ) {
                ExpressionVisitor* v = new ExpressionVisitor(m_duContext.data());
                v->m_forceGlobalSearching = true;
                v->visitCode(tmpAst);
                if ( v->lastDeclaration() ) {
                    calltips << v->lastDeclaration().data();
                }
                else {
                    kWarning() << "Did not receive a function declaration from expression visitor! Not offering call tips.";
                    kWarning() << "Tried: " << m_guessTypeOfExpression;
                }
                delete v;
            }
            
            QList<Declaration*> realCalltips;
            foreach ( Declaration* current, calltips ) {
                current = Helper::resolveAliasDeclaration(current);
                if ( ! current->isFunctionDeclaration() ) {
                    kDebug() << "Not a function declaration: " << current->toString();
                    continue;
                }
                realCalltips.append(current);
            }
            
            QList<CompletionTreeItemPointer> calltipItems = declarationListToItemList(realCalltips);
            foreach ( CompletionTreeItemPointer current, calltipItems ) {
                kDebug() << "Adding calltip item, at argument:" << m_alreadyGivenParametersCount+1; 
                FunctionDeclarationCompletionItem* item = static_cast<FunctionDeclarationCompletionItem*>(current.data());
                item->setAtArgument(m_alreadyGivenParametersCount + 1);
                item->setDepth(depth());
            }
            
            resultingItems.append(calltipItems);
        }
    else if ( m_operation == PythonCodeCompletionContext::DefineCompletion ) {
        // Find all base classes of the current class context
        if ( m_duContext->type() != DUContext::Class ) {
            kWarning() << "current context is not a class context, not offering define completion";
        }
        else if ( ClassDeclaration* klass = dynamic_cast<ClassDeclaration*>(m_duContext->owner()) ) {
            QList<DUContext*> baseClassContexts = Helper::internalContextsForClass(
                klass->type<StructureType>(), m_duContext->topContext()
            );
            // This class' context is put first in the list, so all functions existing here
            // can be skipped.
            baseClassContexts.removeAll(m_duContext.data());
            baseClassContexts.prepend(m_duContext.data());
            Q_ASSERT(baseClassContexts.size() >= 1);
            QList<IndexedString> existingIdentifiers;
            
            bool isOwnContext = true;
            foreach ( DUContext* c, baseClassContexts ) {
                QList<DeclarationDepthPair> declarations = c->allDeclarations(
                    CursorInRevision::invalid(), m_duContext->topContext(), false
                );
                foreach ( DeclarationDepthPair d, declarations ) {
                    if ( FunctionDeclaration* funcDecl = dynamic_cast<FunctionDeclaration*>(d.first) ) {
                        // python does not have overloads or similar, so comparing the function names is enough.
                        const IndexedString identifier = funcDecl->identifier().identifier();
                        if ( isOwnContext ) {
                            existingIdentifiers << identifier;
                        }
                            
                        if ( existingIdentifiers.contains(identifier) ) {
                            continue;
                        }
                        existingIdentifiers << identifier;
                        QStringList argumentNames;
                        DUContext* argumentsContext = DUChainUtils::getArgumentContext(funcDecl);
                        foreach ( Declaration* argument, argumentsContext->localDeclarations() ) {
                            argumentNames << argument->identifier().toString();
                        }
                        resultingItems << CompletionTreeItemPointer(new ImplementFunctionCompletionItem(
                            funcDecl->identifier().toString(), argumentNames, m_indent)
                        );
                    }
                }
                isOwnContext = false;
            }
        }
    }
    else if ( m_operation == PythonCodeCompletionContext::RaiseExceptionCompletion ) {
        kDebug() << "Finding items for raise statement";
        ReferencedTopDUContext ctx = Helper::getDocumentationFileContext();
        QList< Declaration* > declarations = ctx->findDeclarations(QualifiedIdentifier("BaseException"));
        if ( declarations.isEmpty() || ! declarations.first()->abstractType() ) {
            kDebug() << "No valid exception classes found, aborting";
        }
        else {
            Declaration* base = declarations.first();
            IndexedType baseType = base->abstractType()->indexed();
            QList<DeclarationDepthPair> validDeclarations;
            ClassDeclaration* current = 0;
            StructureType::Ptr type;
            foreach ( DeclarationDepthPair d, m_duContext->topContext()->allDeclarations(CursorInRevision::invalid(), m_duContext->topContext()) ) {
                if ( ( current = dynamic_cast<ClassDeclaration*>(d.first) ) ) {
                    if ( current->baseClassesSize() ) {
                        FOREACH_FUNCTION( const BaseClassInstance& base, current->baseClasses ) {
                            if ( base.baseClass == baseType ) {
                                validDeclarations << d;
                            }
                        }
                    }
                }
            }
            resultingItems.append(declarationListToItemList(validDeclarations));
        }
    }
    else if ( m_operation == PythonCodeCompletionContext::ImportFileCompletion ) {
        kDebug() << "Preparing to do autocompletion for import...";
        m_maxFolderScanDepth = 1;
        resultingItems << includeItemsForSubmodule("");
    }
    else if ( m_operation == PythonCodeCompletionContext::ImportSubCompletion ) {
        kDebug() << "Finding items for submodule: " << m_searchImportItemsInModule;
        resultingItems << includeItemsForSubmodule(m_searchImportItemsInModule);
    }
    else if ( m_operation == PythonCodeCompletionContext::InheritanceCompletion ) {
        kDebug() << "InheritanceCompletion";
        QList<DeclarationDepthPair> declarations = m_duContext->allDeclarations(m_position, m_duContext->topContext());
        QList<DeclarationDepthPair> remainingDeclarations;
        foreach ( DeclarationDepthPair d, declarations ) {
            Declaration* r = Helper::resolveAliasDeclaration(d.first);
            if ( r and r->identifier().identifier().str().contains("__kdevpythondocumentation_builtin") ) {
                continue;
            }
            if ( r && dynamic_cast<ClassDeclaration*>(r) ) {
                remainingDeclarations << d;
            }
        }
        resultingItems.append(declarationListToItemList(remainingDeclarations));
    }
    else if ( m_operation == PythonCodeCompletionContext::MemberAccessCompletion ) {
        KDevPG::MemoryPool pool;
        AstBuilder* builder = new AstBuilder(&pool);
        CodeAst* tmpAst = builder->parse(KUrl(), m_guessTypeOfExpression);
        if ( tmpAst ) {
            ExpressionVisitor* v = new ExpressionVisitor(m_duContext.data());
            v->m_forceGlobalSearching = true;
            v->visitCode(tmpAst);
            if ( v->lastType() ) {
                kDebug() << v->lastType()->toString();
                resultingItems << getCompletionItemsForType(v->lastType());
            }
            else {
                kWarning() << "Did not receive a type from expression visitor! Not offering autocompletion.";
            }
            delete v;
        }
        else {
            kWarning() << "Completion requested for syntactically invalid expression, not offering anything";
        }
        delete builder;
    }
    else {
        // it's stupid to display a 3-letter completion item on manually invoked code completion and makes everything look crowded
        if ( m_operation == PythonCodeCompletionContext::NewStatementCompletion && ! fullCompletion ) {
            QStringList keywordItems;
            keywordItems << "def" << "class" << "lambda" << "global" << "print" << "import" << "from" << "while" << "for" << "yield" << "return";
            foreach ( const QString& current, keywordItems ) {
                KeywordItem* k = new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), current + " ");
                resultingItems << CompletionTreeItemPointer(k);
            }
        }
        if ( abort ) {
            return QList<CompletionTreeItemPointer>();
        }
        QList<DeclarationDepthPair> declarations = m_duContext->allDeclarations(m_position, m_duContext->topContext());
        foreach ( DeclarationDepthPair d, declarations ) {
            if ( d.first and d.first->context()->type() == DUContext::Class ) {
                declarations.removeAll(d);
            }
            if ( d.first and d.first->identifier().identifier().str().contains("__kdevpythondocumentation_builtin") ) {
                declarations.removeAll(d);
            }
        }
        resultingItems.append(declarationListToItemList(declarations));
    }
    
    m_searchingForModule.clear();
    m_searchImportItemsInModule.clear();
    
    return resultingItems;
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::declarationListToItemList(QList<DeclarationDepthPair> declarations, int maxDepth)
{
    QList<CompletionTreeItemPointer> items;
    
    DeclarationPointer currentDeclaration;
    DUChainPointer<Declaration> checkDeclaration;
    int count = declarations.length();
    for ( int i = 0; i < count; i++ ) {
        if ( maxDepth && maxDepth > declarations.at(i).second ) {
            kDebug() << "Skipped completion item because of its depth";
            continue;
        }
        currentDeclaration = DeclarationPointer(declarations.at(i).first);
        
        PythonDeclarationCompletionItem* item = 0;
        AliasDeclaration* alias = dynamic_cast<AliasDeclaration*>(currentDeclaration.data());
        if ( alias ) {
            checkDeclaration = DUChainPointer<Declaration>(alias->aliasedDeclaration().declaration());
        }
        else {
            checkDeclaration = currentDeclaration;
        }
        if ( checkDeclaration ) {
            AbstractType::Ptr type = checkDeclaration->abstractType();
            if ( type && ( type->whichType() == AbstractType::TypeFunction || type->whichType() == AbstractType::TypeStructure ) ) {
                item = new FunctionDeclarationCompletionItem(currentDeclaration, KDevelop::CodeCompletionContext::Ptr(this));
            }
            else {
                item = new PythonDeclarationCompletionItem(currentDeclaration, KDevelop::CodeCompletionContext::Ptr(this));
            }
            items << CompletionTreeItemPointer(item);
        }
    }
    return items;
}

QList< CompletionTreeItemPointer > PythonCodeCompletionContext::declarationListToItemList(QList< Declaration* > declarations)
{
    QList<DeclarationDepthPair> fakeItems;
    foreach ( Declaration* d, declarations ) {
        fakeItems << DeclarationDepthPair(d, 0);
    }
    return declarationListToItemList(fakeItems);
}

QList< CompletionTreeItemPointer > PythonCodeCompletionContext::getCompletionItemsForType(AbstractType::Ptr type)
{
    QList<CompletionTreeItemPointer> result;
    type = Helper::resolveType(type);
    if ( type->whichType() == AbstractType::TypeUnsure ) {
        UnsureType::Ptr unsure = type.cast<UnsureType>();
        int count = unsure->typesSize();
        kDebug() << "Getting completion items for " << count << "types of unsure type " << unsure;
        for ( int i = 0; i < count; i++ ) {
            result.append(getCompletionItemsForOneType(unsure->types()[i].abstractType()));
        }
    }
    else {
        result = getCompletionItemsForOneType(type);
    }
    return result;
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::getCompletionItemsForOneType(AbstractType::Ptr type)
{
    type = Helper::resolveType(type);
    ReferencedTopDUContext builtinTopContext = Helper::getDocumentationFileContext();
    if ( type->whichType() == AbstractType::TypeStructure ) {
        // find properties of class declaration
        TypePtr<StructureType> cls = StructureType::Ptr::dynamicCast(type);
        kDebug() << "Finding completion items for class type";
        if ( ! cls || ! cls->internalContext(m_duContext->topContext()) ) {
            kWarning() << "No class type available, no completion offered";
            kDebug() << cls;
            return QList<CompletionTreeItemPointer>();
        }
        // the PublicOnly will filter out non-explictly defined __get__ etc. functions inherited from object
        QList<DUContext*> searchContexts = Helper::internalContextsForClass(cls, m_duContext->topContext(), Helper::PublicOnly);
        QList<DeclarationDepthPair> keepDeclarations;
        foreach ( const DUContext* currentlySearchedContext, searchContexts ) {
            kDebug() << "searching context " << currentlySearchedContext->scopeIdentifier() << "for autocompletion items";
            QList<DeclarationDepthPair> declarations = currentlySearchedContext->allDeclarations(CursorInRevision::invalid(),
                                                                                                 m_duContext->topContext(),
                                                                                                 false);
            kDebug() << "found" << declarations.length() << "declarations";
            
            // filter out those which are builtin functions, and those which were imported; we don't want those here
            // also, discard all magic functions from autocompletion
            // TODO rework this, it's maybe not the most elegant solution possible
            // TODO rework the magic functions thing, I want them sorted at the end of the list but KTE doesn't seem to allow that
            foreach ( DeclarationDepthPair current, declarations ) {
                if ( current.first->context() != builtinTopContext && ! current.first->identifier().identifier().str().startsWith("__") ) {
                    keepDeclarations.append(current);
                }
                else {
                    kDebug() << "Discarding declaration " << current.first->toString();
                }
            }
        }
        return declarationListToItemList(keepDeclarations);
    }
    
    return QList<CompletionTreeItemPointer>();
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::findIncludeItems(IncludeSearchTarget item)
{
    kDebug() << "TARGET:" << item.directory.pathOrUrl() << item.remainingIdentifiers << item.directory.path();
    QDir currentDirectory(item.directory.path());
    QFileInfoList contents = currentDirectory.entryInfoList(QStringList(), QDir::Files | QDir::Dirs);
    bool atBottom = item.remainingIdentifiers.isEmpty();
    QList<CompletionTreeItemPointer> items;
    
    QString sourceFile;
    
    if ( item.remainingIdentifiers.isEmpty() ) {
        // check for the __init__ file
        QFileInfo initFile(item.directory.path(), "__init__.py");
        if ( initFile.exists() ) {
            IncludeItem init;
            init.basePath = item.directory;
            init.isDirectory = true;
            init.name = "";
            ImportFileItem* importfile = new ImportFileItem(init);
            importfile->moduleName = item.directory.fileName();
            items << CompletionTreeItemPointer(importfile);
            sourceFile = initFile.filePath();
        }
    }
    else {
        QFileInfo file(item.directory.path(), item.remainingIdentifiers.first() + ".py");
        item.remainingIdentifiers.removeFirst();
        kDebug() << " CHECK:" << file.absoluteFilePath();
        if ( file.exists() ) {
            sourceFile = file.absoluteFilePath();
        }
    }
    
    if ( ! sourceFile.isEmpty() ) {
        IndexedString filename(sourceFile);
        TopDUContext* top = DUChain::self()->chainForDocument(filename);
        kDebug() << top;
        DUContext* c = internalContextForDeclaration(top, item.remainingIdentifiers);
        kDebug() << "  GOT:" << c;
        if ( c ) {
            items << declarationListToItemList(c->localDeclarations().toList());
        }
        else {
            // do better next time
            DUChain::self()->updateContextForUrl(filename, TopDUContext::AllDeclarationsAndContexts);
        }
    }
    
    if ( atBottom ) {
        // append all python files in the directory
        foreach ( QFileInfo file, contents ) {
            // TODO windows
            if ( file.fileName().startsWith(".") ) {
                continue;
            }
            kDebug() << " > CONTENT:" << file.absolutePath() << file.fileName();
            if ( file.isFile() ) {
                if ( file.fileName().endsWith(".py") || file.fileName().endsWith(".so") ) {
                    IncludeItem fileInclude;
                    fileInclude.basePath = item.directory;
                    fileInclude.isDirectory = false;
                    fileInclude.name = file.fileName().mid(0, file.fileName().length() - 3); // remove ".py"
                    ImportFileItem* import = new ImportFileItem(fileInclude);
                    import->moduleName = fileInclude.name;
                    items << CompletionTreeItemPointer(import);
                }
            }
            else {
                IncludeItem dirInclude;
                dirInclude.basePath = item.directory;
                dirInclude.isDirectory = true;
                dirInclude.name = file.fileName();
                ImportFileItem* import = new ImportFileItem(dirInclude);
                import->moduleName = dirInclude.name;
                items << CompletionTreeItemPointer(import);
            }
        }
    }
    return items;
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::findIncludeItems(QList< Python::IncludeSearchTarget > items)
{
    QList<CompletionTreeItemPointer> results;
    foreach ( const IncludeSearchTarget& item, items ) {
        results << findIncludeItems(item);
    }
    return results;
}

DUContext* PythonCodeCompletionContext::internalContextForDeclaration(TopDUContext* topContext, QStringList remainingIdentifiers)
{
    Declaration* d = 0;
    DUContext* c = topContext;
    if ( remainingIdentifiers.isEmpty() ) {
        return topContext;
    }
    do {
        QList< Declaration* > decls = c->findDeclarations(QualifiedIdentifier(remainingIdentifiers.first()));
        remainingIdentifiers.removeFirst();
        if ( decls.isEmpty() ) {
            return 0;
        }
        d = decls.first();
        if ( (c = d->internalContext()) ) {
            if ( remainingIdentifiers.isEmpty() ) {
                return c;
            }
        }
        else return 0;
        
    } while ( d && ! remainingIdentifiers.isEmpty() );
    return 0;
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::includeItemsForSubmodule(QString submodule)
{
    QList<KUrl> searchPaths = Helper::getSearchPaths(m_workingOnDocument);
    
    QStringList subdirs;
    if ( ! submodule.isEmpty() ) {
        subdirs = submodule.split(".");
    }
    
    Q_ASSERT(! subdirs.contains(""));
    
    QList<IncludeSearchTarget> foundPaths;
    
    // this is a bit tricky. We need to find every path formed like /.../foo/bar for
    // a query string ("submodule" variable) like foo.bar
    // we also need paths like /foo.py, because then bar is probably a module in that file.
    // Thus, we first generate a list of possible paths, then match them against those which actually exist
    // and then gather all the items in those paths.
    
    foreach (KUrl currentPath, searchPaths) {
        kDebug() << "Searching: " << currentPath << subdirs;
        int identifiersUsed = 0;
        foreach ( QString subdir, subdirs ) {
            currentPath.cd(subdir);
            QFileInfo d(currentPath.path());
            kDebug() << currentPath << d.exists() << d.isDir();
            if ( ! d.exists() || ! d.isDir() ) {
                currentPath.cd("..");
                currentPath.cleanPath();
                break;
            }
            identifiersUsed++;
        }
        QStringList remainingIdentifiers = subdirs.mid(identifiersUsed, -1);
        foundPaths.append(IncludeSearchTarget(currentPath, remainingIdentifiers));
        kDebug() << "Found path:" << currentPath << remainingIdentifiers << subdirs;
    }
    return findIncludeItems(foundPaths);
}

PythonCodeCompletionContext::PythonCodeCompletionContext(DUContextPointer context, const QString& remainingText, 
                                                         QString calledFunction, int depth, int alreadyGivenParameters)
    : CodeCompletionContext(context, remainingText, CursorInRevision::invalid(), depth)
    , m_operation(FunctionCallCompletion)
    , m_itemTypeHint(NoHint)
    , m_guessTypeOfExpression(calledFunction)
    , m_alreadyGivenParametersCount(alreadyGivenParameters)
{
    ExpressionParser p(remainingText);
    summonParentForEventualCall(p.popAll(), remainingText);
}

void PythonCodeCompletionContext::summonParentForEventualCall(TokenList allExpressions, const QString& text)
{
    int offset = 0;
    while ( true ) {
        QPair<int, int> nextCall = allExpressions.nextIndexOfStatus(ExpressionParser::EventualCallFound, offset);
        kDebug() << "next call:" << nextCall;
        kDebug() << allExpressions.toString();
        if ( nextCall.first != -1 ) {
            offset = nextCall.first;
            allExpressions.reset(offset);
            TokenListEntry eventualFunction = allExpressions.weakPop();
            kDebug() << eventualFunction.expression << eventualFunction.status;
            // it's only a call if a "(" bracket is followed (<- direction) by an expression.
            if ( eventualFunction.status == ExpressionParser::ExpressionFound ) {
                kDebug() << "Call found! Creating parent-context.";
                // determine the amount of "free" commas in between
                allExpressions.reset();
                int atParameter = 0;
                for ( int i = 0; i < offset-1; i++ ) {
                    TokenListEntry entry = allExpressions.weakPop();
                    if ( entry.status == ExpressionParser::CommaFound ) {
                        atParameter += 1;
                    }
                    // clear the param count for something like "f([1, 2, 3" (it will be cleared when the "[" is read)
                    if ( entry.status == ExpressionParser::InitializerFound || entry.status == ExpressionParser::EventualCallFound ) {
                        atParameter = 0;
                    }
                }
                m_parentContext = new PythonCodeCompletionContext(m_duContext, 
                                                                  text.mid(0, eventualFunction.charOffset),
                                                                  eventualFunction.expression, depth() + 1,
                                                                  atParameter
                                                                 );
                break;
            }
            else continue; // not a call, try the next opening "(" bracket
        }
        else break; // no more eventual calls
    }
    allExpressions.reset(1);
}

// decide what kind of completion will be offered based on the code before the current cursor position
PythonCodeCompletionContext::PythonCodeCompletionContext(DUContextPointer context, const QString& text,
                                                         const KDevelop::CursorInRevision& position, 
                                                         int depth, const PythonCodeCompletionWorker* parent)
    : CodeCompletionContext(context, text, position, depth)
    , m_operation(PythonCodeCompletionContext::DefaultCompletion)
    , m_itemTypeHint(NoHint)
    , worker(parent)
    , m_position(position)
{
    m_workingOnDocument = context->topContext()->url().toUrl();
    QString textWithoutStrings = CodeHelpers::killStrings(text);
    
    kDebug() << text << position << context->localScopeIdentifier().toString() << context->range();
    
    // check if the current position is inside a multi-line comment / string -> no completion if this is the case
    if ( CodeHelpers::endsInsideCommend(text) ) {
        m_operation = PythonCodeCompletionContext::NoCompletion;
        return;
    }
    
    // The expression parser used to determine the type of completion required.
    ExpressionParser parser(textWithoutStrings);
    TokenList allExpressions = parser.popAll();
    allExpressions.reset(1);
    allExpressions.length();
    ExpressionParser::Status firstStatus = allExpressions.last().status;
    
    // TODO reuse already calculated information
    FileIndentInformation indents(text);
    
    DUContext* currentlyChecked = context.data();
    int currentlyCheckedLine = position.line;
    {
        DUChainReadLocker lock(DUChain::lock());
        while ( currentlyChecked == context.data() && currentlyCheckedLine >= 0 ) {
            currentlyChecked = context->topContext()->findContextAt(CursorInRevision(currentlyCheckedLine, 0), true);
            currentlyCheckedLine -= 1;
        }
        while ( currentlyChecked && context->parentContextOf(currentlyChecked) ) {
            kDebug() << "checking:" << currentlyChecked->range() << currentlyChecked->type();
            // FIXME: "<=" is not really good, it must be exactly one indent-level less
            if (    indents.indentForLine(indents.linesCount()-1-(position.line-currentlyChecked->range().start.line)) 
                 <= indents.indentForLine(indents.linesCount()-1) )
            {
                kDebug() << "changing context to" << currentlyChecked->range() << ( currentlyChecked->type() == DUContext::Class );
                context = currentlyChecked;
                break;
            }
            currentlyChecked = currentlyChecked->parentContext();
        }
    }
    
    m_duContext = context;
    
    QList<ExpressionParser::Status> defKeywords;
    defKeywords << ExpressionParser::DefFound << ExpressionParser::ClassFound;
    
    if ( allExpressions.nextIndexOfStatus(ExpressionParser::EventualCallFound).first != -1 ) {
        // 3 is always the case for "def foo(" or class foo(": one names the function, the other is the keyword
        if ( allExpressions.length() == 4 ) {
            if ( allExpressions.at(1).status == ExpressionParser::DefFound ) {
                // The next thing the user probably wants to type are parameters for his function.
                // We cannot offer completion for this.
                m_operation = NoCompletion;
                return;
            }
        }
        if ( allExpressions.length() >= 4 ) {
            // TODO: optimally, filter out classes we already inherit from. That's a bonus, tough.
            if ( allExpressions.at(1).status == ExpressionParser::ClassFound ) {
                // show only items of type "class" for completion
                m_operation = InheritanceCompletion;
                return;
            }
        }
    }
    
    // For something like "func1(3, 5, func2(7, ", we want to show all calltips recursively
    summonParentForEventualCall(allExpressions, textWithoutStrings);
    
    if ( firstStatus == ExpressionParser::MeaninglessKeywordFound ) {
        m_operation = DefaultCompletion;
        return;
    }
    
    if ( firstStatus == ExpressionParser::InFound ) {
        m_operation = DefaultCompletion;
        m_itemTypeHint = IterableRequested;
        return;
    }
    
    if ( firstStatus == ExpressionParser::NoCompletionKeywordFound ) {
        m_operation = NoCompletion;
        return;
    }
    
    if ( firstStatus == ExpressionParser::RaiseFound || firstStatus == ExpressionParser::ExceptFound ) {
        m_operation = RaiseExceptionCompletion;
        return;
    }

    if ( firstStatus == ExpressionParser::NothingFound ) {
        m_operation = NewStatementCompletion;
        return;
    }
    
    if ( firstStatus == ExpressionParser::DefFound ) {
        if ( context->type() == DUContext::Class ) {
            m_indent = QString(" ").repeated(indents.indentForLine(indents.linesCount()-1));
            m_operation = DefineCompletion;
        }
        else {
            kDebug() << "def outside class context";
            m_operation = NoCompletion;
        }
        return;
    }
    
    // The "def in class context" case is handled above already
    if ( defKeywords.contains(firstStatus) ) {
        m_operation = NoCompletion;
        return;
    }
    
    if ( firstStatus == ExpressionParser::ForFound ) {
        int offset = allExpressions.length() - 2; // one for the "for", and one for the general off-by-one thing
        QPair<int, int> nextInitializer = allExpressions.nextIndexOfStatus(ExpressionParser::InitializerFound);
        if ( nextInitializer.first == -1 ) {
            // no opening bracket, so no generator completion.
            m_operation = NoCompletion;
            return;
        }
        // check that all statements in between are part of a generator initializer list
        bool ok = true;
        QString text;
        int currentOffset = 0;
        while ( ok && offset > currentOffset ) {
            ok = allExpressions.at(offset).status == ExpressionParser::ExpressionFound;
            if ( ! ok ) break;
            text.prepend(allExpressions.at(offset).expression);
            offset -= 1;
            ok = allExpressions.at(offset).status == ExpressionParser::CommaFound;
            // the last expression must *not* have a comma
            currentOffset = allExpressions.length() - nextInitializer.first;
            if ( ! ok && currentOffset == offset ) {
                ok = true;
            }
            offset -= 1;
        }
        if ( ok ) {
            m_guessTypeOfExpression = text;
            m_operation = GeneratorVariableCompletion;
            return;
        }
        else {
            m_operation = NoCompletion;
            return;
        }
    }
    
    QPair<int, int> import = allExpressions.nextIndexOfStatus(ExpressionParser::ImportFound);
    QPair<int, int> from = allExpressions.nextIndexOfStatus(ExpressionParser::FromFound);
    int importIndex = import.first;
    int fromIndex = from.first;
    
    if ( importIndex != -1 && fromIndex != -1 ) {
        // it's either "import ... from" or "from ... import"
        // we treat both in exactly the same way. This is not quite correct, as python
        // forbids some combinations. TODO fix this.
        
        // There's two relevant pieces of text, the one between the "from...import" (or "import...from"),
        // and the one behind the "import" or the "from" (at the end of the line).
        // Both need to be extracted and passed to the completion computer.
        QString firstPiece, secondPiece;
        if ( fromIndex > importIndex ) {
            // import ... from
            if ( fromIndex == allExpressions.length() ) {
                // The "from" is the last item in the chain
                m_operation = ImportFileCompletion;
                return;
            }
            firstPiece = allExpressions.at(allExpressions.length() - importIndex - 1).expression;
        }
        else {
            firstPiece = allExpressions.at(allExpressions.length() - fromIndex - 1).expression;
        }
        // might be "from foo import bar as baz, bang as foobar, ..."
        if ( allExpressions.length() > 4 && firstStatus == ExpressionParser::MemberAccessFound ) {
            secondPiece = allExpressions.at(allExpressions.length() - 2).expression;
        }
        
        if ( fromIndex < importIndex ) {
            // if it's "from ... import", swap the two pieces.
            qSwap(firstPiece, secondPiece);
        }
        
        if ( firstPiece.isEmpty() ) {
            m_searchImportItemsInModule = secondPiece;
        }
        else if ( secondPiece.isEmpty() ) {
            m_searchImportItemsInModule = firstPiece;
        }
        else {
            m_searchImportItemsInModule = firstPiece + "." + secondPiece;
        }
        kDebug() << firstPiece << secondPiece;
        kDebug() << "Got submodule to search:" << m_searchImportItemsInModule << "from text" << textWithoutStrings;
        m_operation = ImportSubCompletion;
        return;
    }
    
    if ( firstStatus == ExpressionParser::FromFound || firstStatus == ExpressionParser::ImportFound ) {
        // it's either "from ..." or "import ...", which both come down to the same completion offered
        m_operation = ImportFileCompletion;
        return;
    }
    
    if ( fromIndex != -1 || importIndex != -1 ) {
        if ( firstStatus == ExpressionParser::CommaFound ) {
            // "import foo.bar, "
            m_operation = ImportFileCompletion;
            return;
        }
        if ( firstStatus == ExpressionParser::MemberAccessFound ) {
            m_operation = ImportSubCompletion;
            m_searchImportItemsInModule = allExpressions.at(allExpressions.length() - 2).expression;
            return;
        }
        // "from foo ..."
        m_operation = NoCompletion;
        return;
    }
    
    if ( firstStatus == ExpressionParser::MemberAccessFound ) {
        TokenListEntry item = allExpressions.weakPop();
        if ( item.status == ExpressionParser::ExpressionFound ) {
            m_guessTypeOfExpression = item.expression;
            m_operation = MemberAccessCompletion;
        }
        else {
            m_operation = NoCompletion;
        }
        return;
    }
}

}
