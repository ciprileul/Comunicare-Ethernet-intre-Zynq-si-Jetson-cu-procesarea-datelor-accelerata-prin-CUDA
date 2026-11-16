# jetson/

Codul care rulează pe NVIDIA Jetson Orin Nano (Linux / JetPack).

Ce va conține:
- serverul TCP care primește datele de la Zynq și trimite rezultatele înapoi;
- procesarea textului (litere mici → litere mari);
- cele minim 3 operații pe imagine, fiecare în două variante: CPU (referință) și CUDA (kerneluri proprii);
- măsurarea timpilor (CPU, GPU doar kernel, GPU cu transferuri).
