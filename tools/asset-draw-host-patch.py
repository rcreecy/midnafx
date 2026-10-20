"""Check/apply/reverse the isolated Aurora draw-encoding diagnostic patch.

This is not a production host change. It retains debug labels in release-mode
frame packets and prevents batching across traced shape boundaries.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PIN = '7f2801cd0133c9333eadb4e2e6b24100c328d328'
PATCH = ROOT/'patches/aurora-asset-draw-trace.patch'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aurora-dir',type=Path,default=ROOT/'upstream/dusklight/extern/aurora')
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--apply',action='store_true')
    mode.add_argument('--reverse',action='store_true')
    args=parser.parse_args()
    def git(*argv):
        return subprocess.run(['git','-C',str(args.aurora_dir),*argv],capture_output=True,text=True)
    revision=git('rev-parse','HEAD')
    if revision.returncode or revision.stdout.strip()!=PIN:
        parser.error(f'Aurora must be pinned to {PIN}')
    forward=git('apply','--check',str(PATCH))
    reverse=git('apply','--reverse','--check',str(PATCH))
    if forward.returncode and reverse.returncode:
        parser.error('Patch neither applies nor reverses cleanly; preserve existing edits and inspect conflicts')
    if args.apply and forward.returncode==0:
        result=git('apply',str(PATCH))
    elif args.reverse and reverse.returncode==0:
        result=git('apply','--reverse',str(PATCH))
    else:
        print('Experimental patch is already applied' if reverse.returncode==0 else 'Experimental patch applies cleanly')
        return
    if result.returncode: parser.error(result.stderr.strip())
    print('Experimental patch applied' if args.apply else 'Experimental patch reversed')

if __name__=='__main__': main()
