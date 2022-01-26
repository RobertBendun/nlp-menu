from typing import Callable, NoReturn
import requests
import re
import shlex

Port = 9095

Email = re.compile(r"([A-Za-z0-9]+[.-_])*[A-Za-z0-9]+@[A-Za-z0-9-]+(\.[A-Z|a-z]{2,})+")
Url = re.compile(r'(http|ftp|https):\/\/([\w\-_]+(?:(?:\.[\w\-_]+)+))([\w\-\.,@?^=%&:/~\+#]*[\w\-\@?^=%&/~\+#])?')


def get_suggestions(text : str) -> list[tuple[str, Callable]]:
    text = text.strip()
    suggestions = []

    if Email.fullmatch(text):
        suggestions.append(("Wyślij wiadomość do " + text, lambda: action_email(text)))
        return suggestions

    if Url.fullmatch(text):
        suggestions.append(('Otwórz adres ' + text, lambda: action_web(text)))

    # requests.post(f"http://localhost:{Port}/", json={ "input": text })
    return suggestions

def action_email(address : str) -> str:
    print(f'thunderbird -compose "to=\'{address}\'"')


def action_web(url : str) -> str:
    print(f'firefox {shlex.quote(url)}')
