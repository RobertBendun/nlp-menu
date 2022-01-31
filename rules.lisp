(action
	((one-of "otwórz" "odtwórz" "graj" "wyświetl")
		(find-all-with-extension ("mp4" "mkv") "~/downloads"))
	("mpv" 2))

(action
	((one-of "wyślij" "napisz") (one-of "wiadomość" "maila" "email") "do" match-email)
	("thunderbird -compose \"to'" 4 "'\""))

(action
	((one-of "otwórz" "edytuj") "skrypt" (find-all-executable "~/.local/bin"))
	("alacritty -e vim " 3))
