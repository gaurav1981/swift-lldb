//===-- ObjCLanguageRuntime.cpp ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "clang/AST/Type.h"

#include "lldb/Core/Log.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Target/ObjCLanguageRuntime.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
ObjCLanguageRuntime::~ObjCLanguageRuntime()
{
}

ObjCLanguageRuntime::ObjCLanguageRuntime (Process *process) :
    LanguageRuntime (process)
{

}

void
ObjCLanguageRuntime::AddToMethodCache (lldb::addr_t class_addr, lldb::addr_t selector, lldb::addr_t impl_addr)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_STEP));
    if (log)
    {
        log->Printf ("Caching: class 0x%llx selector 0x%llx implementation 0x%llx.", class_addr, selector, impl_addr);
    }
    m_impl_cache.insert (std::pair<ClassAndSel,lldb::addr_t> (ClassAndSel(class_addr, selector), impl_addr));
}

lldb::addr_t
ObjCLanguageRuntime::LookupInMethodCache (lldb::addr_t class_addr, lldb::addr_t selector)
{
    MsgImplMap::iterator pos, end = m_impl_cache.end();
    pos = m_impl_cache.find (ClassAndSel(class_addr, selector));
    if (pos != end)
        return (*pos).second;
    return LLDB_INVALID_ADDRESS;
}

void
ObjCLanguageRuntime::AddToClassNameCache (lldb::addr_t class_addr, const char *name, lldb::TypeSP type_sp)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_STEP));
    if (log)
    {
        log->Printf ("Caching: class 0x%llx name: %s.", class_addr, name);
    }
    
    TypeAndOrName class_type_or_name;
    
    if (type_sp)
        class_type_or_name.SetTypeSP (type_sp);
    else if (name && *name != '\0')
        class_type_or_name.SetName (name);
    else 
        return;
    m_class_name_cache.insert (std::pair<lldb::addr_t,TypeAndOrName> (class_addr, class_type_or_name));
}

void
ObjCLanguageRuntime::AddToClassNameCache (lldb::addr_t class_addr, const TypeAndOrName &class_type_or_name)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_STEP));
    if (log)
    {
        log->Printf ("Caching: class 0x%llx name: %s.", class_addr, class_type_or_name.GetName().AsCString());
    }
    
    m_class_name_cache.insert (std::pair<lldb::addr_t,TypeAndOrName> (class_addr, class_type_or_name));
}

TypeAndOrName
ObjCLanguageRuntime::LookupInClassNameCache (lldb::addr_t class_addr)
{
    ClassNameMap::iterator pos, end = m_class_name_cache.end();
    pos = m_class_name_cache.find (class_addr);
    if (pos != end)
        return (*pos).second;
    return TypeAndOrName ();
}

size_t
ObjCLanguageRuntime::GetByteOffsetForIvar (ClangASTType &parent_qual_type, const char *ivar_name)
{
    return LLDB_INVALID_IVAR_OFFSET;
}


uint32_t
ObjCLanguageRuntime::ParseMethodName (const char *name, 
                                      ConstString *class_name,              // Class name (with category if any)
                                      ConstString *selector_name,           // selector on its own
                                      ConstString *name_sans_category,      // Full function prototype with no category
                                      ConstString *class_name_sans_category)// Class name with no category (or empty if no category as answer will be in "class_name"
{
    if (class_name)
        class_name->Clear();
    if (selector_name)
        selector_name->Clear();
    if (name_sans_category)
        name_sans_category->Clear();
    if (class_name_sans_category)
        class_name_sans_category->Clear();
    
    uint32_t result = 0;

    if (IsPossibleObjCMethodName (name))
    {
        int name_len = strlen (name);
        // Objective C methods must have at least:
        //      "-[" or "+[" prefix
        //      One character for a class name
        //      One character for the space between the class name
        //      One character for the method name
        //      "]" suffix
        if (name_len >= 6 && name[name_len - 1] == ']')
        {
            const char *selector_name_ptr = strchr (name, ' ');
            if (selector_name_ptr)
            {
                if (class_name)
                {
                    class_name->SetCStringWithLength (name + 2, selector_name_ptr - name - 2);
                    ++result;
                }    
                
                // Skip the space
                ++selector_name_ptr;
                // Extract the objective C basename and add it to the
                // accelerator tables
                size_t selector_name_len = name_len - (selector_name_ptr - name) - 1;
                if (selector_name)
                {
                    selector_name->SetCStringWithLength (selector_name_ptr, selector_name_len);                                
                    ++result;
                }
                
                // Also see if this is a "category" on our class.  If so strip off the category name,
                // and add the class name without it to the basename table. 
                
                if (name_sans_category || class_name_sans_category)
                {
                    const char *open_paren = strchr (name, '(');
                    if (open_paren)
                    {
                        if (class_name_sans_category)
                        {
                            class_name_sans_category->SetCStringWithLength (name + 2, open_paren - name - 2);
                            ++result;
                        }
                        
                        if (name_sans_category)
                        {
                            const char *close_paren = strchr (open_paren, ')');
                            if (open_paren < close_paren)
                            {
                                std::string buffer (name, open_paren - name);
                                buffer.append (close_paren + 1);
                                name_sans_category->SetCString (buffer.c_str());
                                ++result;
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}
