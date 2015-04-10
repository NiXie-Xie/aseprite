// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_DOCUMENT_ACCESS_H_INCLUDED
#define APP_DOCUMENT_ACCESS_H_INCLUDED
#pragma once

#include "base/exception.h"
#include "app/context.h"
#include "app/document.h"

#include <exception>

namespace app {

  class LockedDocumentException : public base::Exception {
  public:
    LockedDocumentException() throw()
    : base::Exception("Cannot read or write the active document.\n"
                      "It is locked by a background task.\n"
                      "Try again later.") { }
  };

  // This class acts like a wrapper for the given document.  It's
  // specialized by DocumentReader/Writer to handle document read/write
  // locks.
  class DocumentAccess {
  public:
    DocumentAccess() : m_document(NULL) { }
    DocumentAccess(const DocumentAccess& copy) : m_document(copy.m_document) { }
    explicit DocumentAccess(Document* document) : m_document(document) { }
    ~DocumentAccess() { }

    DocumentAccess& operator=(const DocumentAccess& copy)
    {
      m_document = copy.m_document;
      return *this;
    }

    operator Document* () { return m_document; }
    operator const Document* () const { return m_document; }

    Document* operator->()
    {
      ASSERT(m_document != NULL);
      return m_document;
    }

    const Document* operator->() const
    {
      ASSERT(m_document != NULL);
      return m_document;
    }

  protected:
    Document* m_document;
  };

  // Class to view the document's state. Its constructor request a
  // reader-lock of the document, or throws an exception in case that
  // the lock cannot be obtained.
  class DocumentReader : public DocumentAccess {
  public:
    DocumentReader()
    {
    }

    explicit DocumentReader(Document* document, int timeout)
      : DocumentAccess(document)
    {
      if (m_document && !m_document->lock(Document::ReadLock, timeout))
        throw LockedDocumentException();
    }

    explicit DocumentReader(const DocumentReader& copy, int timeout)
      : DocumentAccess(copy)
    {
      if (m_document && !m_document->lock(Document::ReadLock, timeout))
        throw LockedDocumentException();
    }

    ~DocumentReader()
    {
      // unlock the document
      if (m_document)
        m_document->unlock();
    }

  private:
    // Disable operator=
    DocumentReader& operator=(const DocumentReader&);
  };

  // Class to modify the document's state. Its constructor request a
  // writer-lock of the document, or throws an exception in case that
  // the lock cannot be obtained. Also, it contains a special
  // constructor that receives a DocumentReader, to elevate the
  // reader-lock to writer-lock.
  class DocumentWriter : public DocumentAccess {
  public:
    DocumentWriter()
      : m_from_reader(false)
      , m_locked(false)
    {
    }

    explicit DocumentWriter(Document* document, int timeout)
      : DocumentAccess(document)
      , m_from_reader(false)
      , m_locked(false)
    {
      if (m_document) {
        if (!m_document->lock(Document::WriteLock, timeout))
          throw LockedDocumentException();

        m_locked = true;
      }
    }

    // Constructor that can be used to elevate the given reader-lock to
    // writer permission.
    explicit DocumentWriter(const DocumentReader& document, int timeout)
      : DocumentAccess(document)
      , m_from_reader(true)
      , m_locked(false)
    {
      if (m_document) {
        if (!m_document->lockToWrite(timeout))
          throw LockedDocumentException();

        m_locked = true;
      }
    }

    ~DocumentWriter()
    {
      unlockWriter();
    }

  protected:

    void unlockWriter()
    {
      if (m_document && m_locked) {
        if (m_from_reader)
          m_document->unlockToRead();
        else
          m_document->unlock();
        m_locked = false;
      }
    }

  private:
    bool m_from_reader;
    bool m_locked;

    // Non-copyable
    DocumentWriter(const DocumentWriter&);
    DocumentWriter& operator=(const DocumentWriter&);
    DocumentWriter& operator=(const DocumentReader&);
  };

  // Used to destroy the active document in the context.
  class DocumentDestroyer : public DocumentWriter {
  public:
    explicit DocumentDestroyer(Context* context, Document* document, int timeout)
      : DocumentWriter(document, timeout)
    {
    }

    void destroyDocument()
    {
      ASSERT(m_document != NULL);

      m_document->close();
      unlockWriter();

      delete m_document;
      m_document = NULL;
    }

  };

} // namespace app

#endif
