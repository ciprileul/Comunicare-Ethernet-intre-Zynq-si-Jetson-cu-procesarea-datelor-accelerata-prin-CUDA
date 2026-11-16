# jetson/

Codul care ruleaza pe NVIDIA Jetson Orin Nano (Linux / JetPack).

Ce va contine:
- serverul TCP care primeste datele de la Zynq si trimite rezultatele inapoi;
- procesarea textului (litere mici -> litere mari);
- cele minim 3 operatii pe imagine, fiecare in doua variante: CPU (referinta) si CUDA (kerneluri proprii);
- masurarea timpilor (CPU, GPU doar kernel, GPU cu transferuri);
- Makefile-ul de compilare (nvcc).

Sugestie de organizare: `src/` pentru surse, `bin/` pentru executabile (ignorat de Git).
