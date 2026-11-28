import sys, pbm
def runs_h(w,h,rows,minlen):
    out=[]
    for y in range(h):
        x=0
        while x<w:
            if pbm.px(rows,x,y):
                s=x
                while x<w and pbm.px(rows,x,y): x+=1
                if x-s>=minlen: out.append((y,s,x-1))
            else: x+=1
    return out
def runs_v(w,h,rows,minlen):
    out=[]
    for x in range(w):
        y=0
        while y<h:
            if pbm.px(rows,x,y):
                s=y
                while y<h and pbm.px(rows,x,y): y+=1
                if y-s>=minlen: out.append((x,s,y-1))
            else: y+=1
    return out
if __name__=="__main__":
    w,h,rows=pbm.read_pbm(sys.argv[1])
    print("H runs"); 
    for r in runs_h(w,h,rows,int(sys.argv[2])): print(r)
    print("V runs")
    for r in runs_v(w,h,rows,int(sys.argv[3])): print(r)
