(define (script-fu-erase-rows img drawable orientation which type)
  (let* ((width (car (gimp-drawable-width drawable)))
	 (height (car (gimp-drawable-height drawable))))
    (gimp-undo-push-group-start img)
    (letrec ((loop (lambda (i max)
		     (if (< i max)
			 (begin
			   (if (= orientation 0)
			       (gimp-rect-select img 0 i width 1 REPLACE FALSE 0)
			       (gimp-rect-select img i 0 1 height REPLACE FALSE 0))
			   (if (= type 0)
			       (gimp-edit-clear drawable)
			       (gimp-edit-fill drawable BACKGROUND-FILL))
			   (loop (+ i 2) max))))))
      (loop (if (= which 0)
		0
		1)
	    (if (= orientation 0)
		height
		width)))
    (gimp-selection-none img)
    (gimp-undo-push-group-end img)
    (gimp-displays-flush)))

(script-fu-register "script-fu-erase-rows"
		    _"<Image>/Script-Fu/Alchemy/_Erase every other Row..."
		    "Erase every other row/column with the background color"
		    "Federico Mena Quintero"
		    "Federico Mena Quintero"
		    "June 1997"
		    "RGB* GRAY* INDEXED*"
		    SF-IMAGE "Image" 0
		    SF-DRAWABLE "Drawable" 0
		    SF-OPTION _"Rows/Cols" '(_"Rows" _"Columns")
		    SF-OPTION _"Even/Odd"  '(_"Even" _"Odd")
		    SF-OPTION _"Erase/Fill"  '(_"Erase" _"Fill with BG"))

