#include "scanner/util/util.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h> /* PATH_MAX */
#include <string.h>
#include <sys/stat.h> /* mkdir(2) */
#include <unistd.h>   /* access(2) */
#include <cstdarg>
#include <iostream>
#include <fstream>
#include <sstream>
#include <wordexp.h>

namespace scanner {
// Stolen from
// https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950
int mkdir_p(const char* path, mode_t mode) {
  /* Adapted from http://stackoverflow.com/a/2336245/119527 */
  const size_t len = strlen(path);
  char _path[PATH_MAX];
  char* p;

  errno = 0;

  /* Copy string so its mutable */
  if (len > sizeof(_path) - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }
  strcpy(_path, path);

  /* Iterate the string */
  for (p = _path + 1; *p; p++) {
    if (*p == '/') {
      /* Temporarily truncate */
      *p = '\0';
      /* check if file exists before mkdir to avoid EACCES */
      if (access(_path, F_OK) != 0) {
        /* fail if error is anything but file does not exist */
        if (errno != ENOENT) {
          return -1;
        }
        if (mkdir(_path, mode) != 0) {
          if (errno != EEXIST) return -1;
        }
      }

      *p = '/';
    }
  }

  if (mkdir(_path, mode) != 0) {
    if (errno != EEXIST) return -1;
  }

  return 0;
}

void temp_file(FILE** fp, std::string& name) {
  char n[] = "/tmp/scannerXXXXXX";
  int fd = mkstemp(n);
  *fp = fdopen(fd, "wb+");
  name = std::string(n);
}

void temp_file(std::string& name) {
  FILE* fp;
  temp_file(&fp, name);
  fclose(fp);
}

void temp_dir(std::string& name) {
  char n[] = "/tmp/scannerXXXXXX";
  (void)mkdtemp(n);
  name = std::string(n);
}

std::string done_file_path(const std::string& path) {
  char* s1 = new char[path.size() + 1];
  memcpy(s1, path.c_str(), path.size() + 1);
  char* dir = dirname(s1);

  char* s2 = new char[path.size() + 1];
  memcpy(s2, path.c_str(), path.size() + 1);
  char* base = basename(s2);

  std::string done_path = std::string(dir) + "/." + base + ".done";

  delete[] s1;
  delete[] s2;

  return done_path;
}

void download(const std::string& url, const std::string& local_path) {
  // mkdir first
  char* s = new char[local_path.size() + 1];
  memcpy(s, local_path.c_str(), local_path.size() + 1);
  char* d = dirname(s);
  mkdir_p(d, S_IRWXU);
  delete[] s;

  std::ostringstream strm;
  strm << "wget " << url << " -O " << local_path;
  int rc = std::system(strm.str().c_str());
  LOG_IF(FATAL, !(WIFEXITED(rc) != 0 && WEXITSTATUS(rc) == 0))
      << "wget failed for url " << url;

  // Touch done file
  std::string done_path = done_file_path(local_path);
  std::fstream fs;
  fs.open(done_path, std::ios::out);
  fs << "done";
  fs.close();
}

std::string download_temp(const std::string& url) {
  std::string local_video_path;
  scanner::temp_file(local_video_path);
  scanner::download(url, local_video_path);
  return local_video_path;
}

std::vector<uint8_t> read_entire_file(const std::string& file_name) {
  std::ifstream file(file_name, std::ios::ate | std::ios::binary);
  size_t file_size = file.tellg();
  file.clear();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data;
  data.reserve(file_size);
  data.assign(std::istreambuf_iterator<char>(file),
              std::istreambuf_iterator<char>());
  return data;
}

void cached_dir(const std::string& name, std::string& full_path) {
  wordexp_t p;
  char** w;
  int i;
  full_path = "~/.scanner/resources/" + name;
  wordexp(full_path.c_str(), &p, 0);
  w = p.we_wordv;
  full_path = w[0];
  wordfree(&p);

  mkdir_p(full_path.c_str(), S_IRWXU);
}

void download_if_uncached(const std::string& url,
                          const std::string& cache_path) {
  // Check if done file exists
  std::string done_path = done_file_path(cache_path);
  struct stat stat_buf;
  int rc = stat(done_path.c_str(), &stat_buf);
  if (rc != 0) {
    // If not, download it
    download(url, cache_path);
  }
}

}
