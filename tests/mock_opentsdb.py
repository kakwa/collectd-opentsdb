from flask import Flask
from flask import request
app = Flask(__name__)
from pprint import pprint


@app.route('/api/put', methods=['POST'])
def hello():
    content = request.get_json(silent=True)
    pprint(content)
    return "{OKaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa}"

if __name__ == '__main__':
    app.run()
