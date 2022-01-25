# nlp-menu

# Dostępne komendy

- [ ] Wpisanie adresu powoduje otwarcie przeglądarki z podanym adresem
- [ ] Wpisanie adresu email powoduje otwarcie klienta pocztowego
- [ ] "Otwórz ..." podowuje wszystkie akcje powyżej opisane
- [ ] "Odtwórz ..." powoduje odtworzenie filmu znalezionego na dysku
- [ ] "Zamknij ..." powoduje zamknięcie wszystkich instancji programu dalej wymienionego
- [ ] "Edytuj ..." umożliwa edytowanie pliku
- [ ] "Edytuj skrypt ..." umożliwa edytowanie skryptu

## Notes from source code

### `dmenu.c`

- `insert()` - After user press key, insert character into the buffer and `match()`
- `match()` - Produces new list based on matched items
