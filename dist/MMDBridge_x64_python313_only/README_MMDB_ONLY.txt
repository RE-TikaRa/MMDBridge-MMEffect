MMDBridge x64 python313 only package

Place these files beside MikuMikuDance.exe.

Included:
- d3d9.dll             MMDBridge front-end
- d3dx9_43.dll         rebuilt D3DX9 proxy
- python313.dll        required runtime
- mmdbridge_*.py       export scripts
- alembic_assign_scripts
- LICENSE.txt          current repository MIT license
- MMDBridge-original-LICENSE.txt
- Alembic-LICENSE.txt
- Python-License.txt

Notes:
- This package is intended for MMDBridge export usage.
- No MMEffect runtime files are included.
- If d3d9_mme.dll is absent, the bridge will fall back to the system D3D9 runtime.
- MMDBridge only scans *.py files placed in the same directory as MikuMikuDance.exe.
