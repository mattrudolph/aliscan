from math import exp,pi,sqrt

#fit function
class myfit1gaus:
    def __init__(self, thr=4.5578424429*5):
        self.thr = -thr

    def __call__(self, x, p):
        if x[0] > -self.thr:
            return 0
        return self.singGaus(x[0], [p[0],p[1],p[2]])

    def singGaus(self, x, p):
        return p[0]*exp(-((x-p[1])*(x-p[1])) /(2*p[2]*p[2]) )/( sqrt(2*pi)*p[2] )

class myfit2gaus:
    def __init__(self, thr=4.5578424429*5):
        self.thr = thr

    def __call__(self, x, p):
        if x[0] > -self.thr:
            return 0
        return self.singGaus(x[0], [p[0],p[1],p[2]]) + self.singGaus(x[0], [p[3],p[4],p[5]])

    def singGaus(self, x, p):
        return p[0]*exp(-((x-p[1])*(x-p[1])) /(2*p[2]*p[2]) )/( sqrt(2*pi)*p[2] )
