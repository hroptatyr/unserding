;;; convert to xml
;; process this file with sxemacs 22.1.10+, M-x eval-current-buffer

(defvar conv-buffer nil)

(defun conv-akas (x)
  (let* ((aka (bbdb-record-aka x)))
    (when aka
      (insert "    <akas>\n")
      (mapfam :result-type 'none
              #'(lambda (a)
                  (insert "      <aka>" a "</aka>\n"))
              aka)
      (insert "    </akas>\n"))))

(defun conv-emails (x)
  (let* ((net (bbdb-record-net x)))
    (when net
      (insert "    <emails>\n")
      (mapfam :result-type 'none
              #'(lambda (a)
                  (insert "      <email>" a "</email>\n"))
              net)
      (insert "    </emails>\n"))))

(defun conv-orgas (x)
  (let* ((orgas (bbdb-record-company x)))
    (when orgas
      (insert "    <organisations>\n")
      (insert "      <organisation>" orgas "</organisation>\n")
      (insert "    </organisations>\n"))))

(defun conv-phones (x)
  (let* ((phones (bbdb-record-phones x)))
    (when phones
      (insert "    <phones>\n")
      (mapfam :result-type 'none
              #'(lambda (a)
                  (let* ((occasion (aref a 0))
                         (number (aref a 1)))
                    (insert "      <phone>\n")
                    (insert "        <occasion>" occasion "</occasion>\n")
                    (insert "        <number>" number "</number>\n")
                    (insert "      </phone>\n")))
              phones)
      (insert "    </phones>\n"))))

(defun conv-addresses (x)
  (let* ((addrs (bbdb-record-addresses x)))
    (when addrs
      (insert "    <addresses>\n")
      (mapfam :result-type 'none
              #'(lambda (a)
                  (let* ((occasion (aref a 0))
                         (street (aref a 1))
                         (town (aref a 2))
                         (state (aref a 3))
                         (postcode (aref a 4))
                         (country (aref a 5)))
                    (insert "      <address>\n")
                    (insert "        <occasion>" occasion "</occasion>\n")
                    (when street
                      (insert "        <street>" (car street) "</street>\n"))
                    (insert "        <town>" town "</town>\n")
                    (insert "        <state>" state "</state>\n")
                    (insert "        <postcode>" postcode "</postcode>\n")
                    (insert "        <country>" country "</country>\n")
                    (insert "      </address>\n")))
              addrs)
      (insert "    </addresses>\n"))))

;; one record
(defun conv-one (x)
  (let* ((gname (bbdb-record-firstname x))
         (sname (bbdb-record-lastname x)))
    (insert "  <entry gname=\"" gname "\" sname=\"" (or sname "") "\">\n")
    (insert "    <fullname>" gname)
    (when sname
      (insert " " sname))
    (insert "</fullname>\n")
    (conv-akas x)
    (conv-emails x)
    (conv-orgas x)
    (conv-phones x)
    (conv-addresses x)
    (insert "  </entry>\n")))

(setq conv-buffer (get-buffer-create "*xbbdb-conv*"))
(with-current-buffer conv-buffer
  (set-buffer-file-coding-system 'utf-8)
  (erase-buffer conv-buffer)
  (insert "<?xml version=\"1.0\" coding=\"utf-8\"?>\n")
  (insert "<bbdb>\n")
  (mapfam :result-type 'none #'conv-one (bbdb-records))
  (insert "</bbdb>\n"))

