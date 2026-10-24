import json
from ed3schema import *
def libs():
    m=json.load(open('out/manifest.json'))
    return [Lib(k,v['txt']) for k,v in m.items()]
