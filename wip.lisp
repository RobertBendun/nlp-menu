; narzędzia systemowe
(action ("zrzut" "ekranu") ("flameshot" "gui"))
(action ("przeglądarka")   ("firefox"))

(action ("zamknij" (processes)) ("killall" last))

; Linki
(action ("github") ("firefox" "https://github.com/RobertBendun"))
(action ("usos")   ("firefox" "https://usosweb.amu.edu.pl"))
(action ("strona") ("firefox" "https://bendun.cc"))

; otwieranie filmów
(action
	((one-of "otwórz" "odtwórz" "wyświetl" "pokaż")
		(find-all-with-extension ("mp4" "mkv") "~/downloads"))
	("mpv" last))

(action
	((one-of "pokaż" "wyświetl" "otwórz") "ostatnie" (one-of "maile" "wiadomości"))
	("thunderbird"))

; modyfikacja skryptów i konfiguracji
(action
	((one-of "otwórz" "edytuj" "modyfikuj") "skrypt" (find-all-executable "~/.local/bin"))
	("alacritty" "-e" "nvim" last))

(action ((one-of "otwórz" "edytuj" "modyfikuj") "aliasy")
	("alacritty" "-e" "nvim" "/home/robert/.config/aliases"))

(action ((one-of "otwórz" "edytuj" "modyfikuj") (one-of "nvim" "vim" "vi" "neovim"))
	("alacritty" "-e" "nvim" "/home/robert/.config/nvim/init.vim"))

; projekty: ~/dev/ oraz ~/repos/
(action ((one-of "otwórz" "edytuj" "modyfikuj") "projekt" (find-dirs "~/dev/"))
				("alacritty" "--working-directory" last))

(action ((one-of "otwórz" "edytuj" "modyfikuj") (find-dirs "~/dev/"))
				("alacritty" "--working-directory" last))

(action ((one-of "otwórz" "edytuj" "modyfikuj") "projekt" (find-dirs "~/repos/"))
				("alacritty" "--working-directory" last))

(action ((one-of "otwórz" "edytuj" "modyfikuj") (find-dirs "~/repos/"))
				("alacritty" "--working-directory" last))
