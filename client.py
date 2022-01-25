import requests

Port = 9095

def get_suggestions(text):
    response = requests.post(f"http://localhost:{Port}/", json={ "input": text })
    print(response.json())
