import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit
import math

# Script to test curve fitting for exp2 approximation
xdat = np.arange(-1,1.04,0.05)
ydat = 2**xdat
#print(ydat)

#plt.scatter(xdat, ydat)

def model_f(x,a,b,c,d):
    return a*x**3+b*x**2+c*x+d

popt, perr = curve_fit(model_f, xdat, ydat, p0=[1,1,1,1])
print(popt)

#yfit = model_f(xdat, popt[0], popt[1], popt[2], popt[3])
#plt.plot(xdat, yfit)

xdat = np.arange(-4,4.04,0.05)
#ydat = 2**xdat
#plt.plot(xdat, ydat)

def fl_fit(val):
    retval = 0
    fmant = val - int(val)
    fint = int(val)
    retval = popt[3]
    retval += popt[2] * fmant
    retval += popt[1] * fmant**2
    retval += popt[0] * fmant**3
    retval *= 2**fint
    return retval

fpos = 16
def fixed_fit(val):
    fixedVal = np.int32(val * 2**fpos)
    retval = 0
    fint = np.int32(fixedVal >> fpos)
    if fixedVal >= 0:
        fmant = np.int32(fixedVal & ~(-1 << fpos))
    else:
        fmant = np.int32(fixedVal | (-1 << fpos))
        fint += 1
    exp_mant = np.int64(fmant)
    retval = np.int32(popt[3] * 2**fpos)
    tempval = np.int64(popt[2] * 2**fpos)
    tempval *= exp_mant
    tempval >>= fpos
    retval += tempval
    tempval = np.int32(popt[1] * 2**fpos)
    exp_mant *= fmant
    exp_mant >>= fpos
    tempval *= exp_mant
    tempval >>= fpos
    retval += tempval
    tempval = np.int32(popt[0] * 2**fpos)
    exp_mant *= fmant
    exp_mant >>= fpos
    tempval *= exp_mant
    tempval >>= fpos
    retval += tempval
    finval = np.int64(1 << (fint + fpos))
    finval *= retval
    finval >>= fpos
    return float(finval)/2**fpos

yfit = []
for dat in xdat:
    yfit.append(fixed_fit(dat))
plt.plot(xdat,yfit)

plt.show()
