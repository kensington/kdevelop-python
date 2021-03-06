/*****************************************************************************
 * Copyright (c) 2007 Piyush verma <piyush.verma@gmail.com>                  *
 * Copyright 2011-2012 Sven Brauch <svenbrauch@googlemail.com>               *
 *                                                                           *
 * Permission is hereby granted, free of charge, to any person obtaining     *
 * a copy of this software and associated documentation files (the           *
 * "Software"), to deal in the Software without restriction, including       *
 * without limitation the rights to use, copy, modify, merge, publish,       *
 * distribute, sublicense, and/or sell copies of the Software, and to        *
 * permit persons to whom the Software is furnished to do so, subject to     *
 * the following conditions:                                                 *
 *                                                                           *
 * The above copyright notice and this permission notice shall be            *
 * included in all copies or substantial portions of the Software.           *
 *                                                                           *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,           *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF        *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                     *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE    *
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION    *
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION     *
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.           *
 *****************************************************************************/
#include "pythoneditorintegrator.h"
#include <ktexteditor/document.h>
#include <ktexteditor/smartrange.h>
#include <ktexteditor/smartinterface.h>

#include <language/editor/documentrange.h>

#include "ast.h"

using namespace KTextEditor;

namespace Python
{

PythonEditorIntegrator::PythonEditorIntegrator(ParseSession* session) :
    m_session(session), m_indentInformationCache(new FileIndentInformation(session->contents()))
{
    
}

PythonEditorIntegrator::~PythonEditorIntegrator() 
{
    delete m_indentInformationCache;
    m_indentInformationCache = 0;
}

ParseSession* PythonEditorIntegrator::parseSession() const
{
    Q_ASSERT(m_session);
    return m_session;
}

CursorInRevision PythonEditorIntegrator::findPosition( Ast* node , Edge edge ) const
{
    Q_ASSERT(node);
    if ( edge == BackEdge )
    {
        // Apparently KTE expects a range to go until _after_ the last character that should be included
        // however the parser calculates endCol as the index _before_ the last included character, so adjust here
        return CursorInRevision( node->endLine, node->endCol+1 );
    }else
    {
        return CursorInRevision( node->startLine, node->startCol );
    }
}

RangeInRevision PythonEditorIntegrator::findRange( Ast * node, RangeEdge edge ) const
{
    Q_UNUSED( edge );
    return RangeInRevision( findPosition( node, FrontEdge ), findPosition( node, BackEdge ) );
}

RangeInRevision PythonEditorIntegrator::findRange( Ast* from, Ast* to ) const
{
    return RangeInRevision( findPosition( from, FrontEdge ), findPosition( to, BackEdge ) );
}

}
