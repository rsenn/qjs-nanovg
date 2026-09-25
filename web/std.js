// Only the write side of std.open(): closing the file triggers a browser download.
export function open(path, mode) {
  if(!mode.startsWith('w')) return null;

  const chunks = [];
  return {
    puts: s => void chunks.push(s),
    close() {
      const a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob(chunks));
      a.download = path;
      a.click();
      URL.revokeObjectURL(a.href);
    },
  };
}
