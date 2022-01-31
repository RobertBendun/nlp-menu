(action
	((one-of "otwórz" "odtwórz" "wyświetl")
		(find-all-with-extension ("mp4" "mkv") "~/downloads"))
	("mpv" last))

(action
	((one-of "otwórz" "pokaż" "wyświetl") "folder")
	("nnn" last))
