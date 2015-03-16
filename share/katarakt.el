;;; katarakt.el --- Synctex support between katarakt and emacs -*- lexical-binding: t -*-

;;; Commentary:

;; This provides a function `katarakt-view', that starts katarakt and
;; jumps to the current source location in the pdf.
;;
;; It also listens to the katarakt dbus signal to automatically find the
;; right source location if Ctrl-<left-mouse-button> is clicked in katarakt.
;;
;; To use it in auctex, add the following to your init file:
;;
;; (define-key 'LaTeX-mode-map ("C-c C-v") 'katarakt-view)

;;; Code:

(require 'dbus)
(require 'tex)

(defgroup katarakt nil
  "Synctex bridge to the katarakt pdf viewer."
  :group 'tex-view)

(defcustom katarakt-command "katarakt"
  "Command name of katarkat."
  :group 'katarakt
  :type 'file)

(defun katarakt--exec-synctex (&rest args)
  "Execute the synctex command with ARGS and return the output as string."
  (with-output-to-string
    (apply 'call-process "synctex" nil (list standard-output nil) nil args)))

(defun katarakt--synctex-get-match (regex output)
  "Returns the first group of the match or nil."
  (when (string-match regex output)
    (match-string 1 output)))

(defun katarakt--synctex-view (input line column output)
  "Parse the output of synctex view -i LINE:COLUMN:INPUT -o OUTPUT.

Returns a list (PAGE X Y)"
  (let* ((synctex-out
          (katarakt--exec-synctex "view"
                                  "-i" (format "%d:%d:%s" line column input)
                                  "-o" output))

         (page (katarakt--synctex-get-match "Page:\\([0-9]+\\)" synctex-out))
         ; deliberately ignore fraction
         (x (katarakt--synctex-get-match "x:\\([0-9]+\\)" synctex-out))
         (y (katarakt--synctex-get-match "y:\\([0-9]+\\)" synctex-out)))

    (if (and page x y)
        (mapcar 'string-to-number (list page x y))
      (error "Could not parse synctex output"))))

(defun katarakt--synctex-edit (pdf page x y)
  "Parse the output of synctex edit -o PAGE:X:Y:PDF.

Returns a list (FILE LINE COLUMN)"
  (let* ((synctex-out
          (katarakt--exec-synctex "edit"
                                  "-o" (format "%d:%d:%d:%s" (+ page 1) x y pdf)))

         (input (katarakt--synctex-get-match "Input:\\(.+\\)\n" synctex-out))
         (line (katarakt--synctex-get-match "Line:\\([0-9]+\\)" synctex-out))
         (column (katarakt--synctex-get-match "Column:\\(-?[0-9]+\\)" synctex-out)))

    (if (and input line column)
        (list input (string-to-number line)
              (if (< (string-to-number column) 0) 0 (string-to-number column)))
      (error "Could not parse synctex output"))))

(defun katarakt--service-name ()
  "Return the service name of the current katarakt process."
  (format "katarakt.pid%d" (process-id (get-process "katarakt"))))

(defun katarakt--call-view (pdf page x y)
  "View the specified location PDF:X:Y in katarakt."
  (dbus-call-method :session (katarakt--service-name) "/"
                    "katarakt.SourceCorrelate" "view" pdf :int32 (- page 1) :int32 x :int32 y))

(defun katarakt--signal-handler (file page x y)
  "Handle dbus signal from katarakt."
  (message "Got signal: %s %d %d %d" file page x y)
  (let ((sync (katarakt--synctex-edit file page x y)))
    (when sync
      (find-file-existing (first sync))
      (goto-line (second sync))
      (move-to-column (third sync)))))

(defvar *katarakt--signal* nil)
(defun katarakt--connect-signal ()
  "Connect to katarakt's synctex edit signal."
  (when *katarakt--signal*
    (dbus-unregister-object *katarakt--signal*))
  (setq *katarakt--signal*
        (dbus-register-signal
         :session (katarakt--service-name) "/"
         "katarakt.SourceCorrelate" "edit" 'katarakt--signal-handler)))

;;;###autoload
(defun katarakt-disconnect-signal ()
  "Disconnect from DBus signals of a katarakt instance."
  (interactive "")
  (when *katarakt--signal*
    (dbus-unregister-object *katarakt--signal*)
    (setq *katarakt--signal* nil)))

(defun katarakt--maybe-start ()
  "Start katarakt if it is not already running."
  ;; if the process is there, but not running: kill it
  (let ((katarakt (get-process "katarakt")))
      (if (and katarakt (process-live-p katarakt))
          katarakt
        (start-process "katarakt" "*katarakt*" katarakt-command)
        (sleep-for 1)                   ; FIXME Ugly. Wait for dbus service somehow
        (katarakt--connect-signal))))

;;;###autoload
(defun katarakt-view ()
  "View the current location in katarakt."
  (interactive "")
  (katarakt--maybe-start)
  (let ((pdf (file-truename (TeX-active-master (TeX-output-extension))))
        (tex (file-truename (buffer-file-name)))
        (line (line-number-at-pos))
        (col (current-column)))
    (let ((sync (katarakt--synctex-view tex line col pdf)))
      (when sync
        (katarakt--call-view pdf (first sync) (second sync) (third sync))))))

(provide 'katarakt)

;;; katarakt.el ends here
