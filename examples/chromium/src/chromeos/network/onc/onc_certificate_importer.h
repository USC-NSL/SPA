// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
#define CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace chromeos {
namespace onc {

// This class handles certificate imports from ONC (both policy and user
// imports) into the certificate store. In particular, the GUID of certificates
// is stored together with the certificate as Nickname.
class CHROMEOS_EXPORT CertificateImporter {
 public:
  enum ParseResult {
    IMPORT_OK,
    IMPORT_INCOMPLETE,
    IMPORT_FAILED,
  };

  // During import with ParseCertificate(), Web trust is only applied to Server
  // and Authority certificates with the TrustBits attribute "Web" if the
  // |allow_trust_imports| permission is granted, otherwise the attribute is
  // ignored.
  explicit CertificateImporter(bool allow_trust_imports);

  // Parses and stores the certificates in |onc_certificates| into the
  // certificate store. If the "Remove" field of a certificate is enabled, then
  // removes the certificate from the store instead of importing. Returns the
  // result of the parse operation. In case of IMPORT_INCOMPLETE, some of the
  // certificates may be stored/removed successfully while others had errors.
  // If |onc_trusted_certificates| is not NULL then it will be filled with the
  // list of certificates that requested the Web trust flag.
  // If no error occurred, returns IMPORT_OK.
  ParseResult ParseAndStoreCertificates(
      const base::ListValue& onc_certificates,
      net::CertificateList* onc_trusted_certificates);

  // Lists the certificates that have the string |label| as their certificate
  // nickname (exact match).
  static void ListCertsWithNickname(const std::string& label,
                                    net::CertificateList* result);

 protected:
  // Deletes any certificate that has the string |label| as its nickname (exact
  // match).
  static bool DeleteCertAndKeyByNickname(const std::string& label);

 private:
  // Parses and stores/removes |certificate| in/from the certificate
  // store. Returns true if the operation succeeded.
  bool ParseAndStoreCertificate(
      const base::DictionaryValue& certificate,
      net::CertificateList* onc_trusted_certificates);

  bool ParseServerOrCaCertificate(
      const std::string& cert_type,
      const std::string& guid,
      const base::DictionaryValue& certificate,
      net::CertificateList* onc_trusted_certificates);

  bool ParseClientCertificate(const std::string& guid,
                              const base::DictionaryValue& certificate);

  // Whether certificates with TrustBits attribute "Web" should be stored with
  // web trust.
  bool allow_trust_imports_;

  DISALLOW_COPY_AND_ASSIGN(CertificateImporter);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
