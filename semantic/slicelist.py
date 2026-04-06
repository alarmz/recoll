
def slicelist(lst, slicesize):
    """slice lst into slicesize chunks and return them in an iterable way. 
    Properly take care of an incomplete last slice. We return a 2-element tuple.
    The first element is the base index for the slice (index of the first element
    in the whole list), the second element is the slice itself."""
    total = len(lst)
    base=0
    slicenum = 0
    while True:
        pos = slicenum * slicesize
        nd = min(pos+slicesize, total)
        sz = nd - pos
        if sz <= 0:
            break
        yield((pos,lst[pos:pos+sz]))
        slicenum += 1


if __name__ == "__main__":
    lst = [1,2,3,4,5,6,7,8,9,10]
    for slice in slicelist(lst, 3):
        print(f"{slice}")
    
